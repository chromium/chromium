// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_xnnpack.h"

#include <algorithm>

#include "base/numerics/checked_math.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "build/buildflag.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

namespace {

String XnnStatusToString(xnn_status status) {
  switch (status) {
    case xnn_status_success:
      return "xnn_status_success";
    case xnn_status_uninitialized:
      return "xnn_status_uninitialized";
    case xnn_status_invalid_parameter:
      return "xnn_status_invalid_parameter";
    case xnn_status_invalid_state:
      return "xnn_status_invalid_state";
    case xnn_status_unsupported_parameter:
      return "xnn_status_unsupported_parameter";
    case xnn_status_unsupported_hardware:
      return "xnn_status_unsupported_hardware";
    case xnn_status_out_of_memory:
      return "xnn_status_out_of_memory";
  }
}

DOMExceptionCode XnnStatusToDOMExceptionCode(xnn_status status) {
  switch (status) {
    case xnn_status_success:
      // This function should only be called with an error.
      NOTREACHED();
      return DOMExceptionCode::kNoError;
    case xnn_status_uninitialized:
      return DOMExceptionCode::kUnknownError;
    case xnn_status_invalid_parameter:
      return DOMExceptionCode::kDataError;
    case xnn_status_invalid_state:
      return DOMExceptionCode::kInvalidStateError;
    case xnn_status_unsupported_parameter:
    case xnn_status_unsupported_hardware:
      return DOMExceptionCode::kNotSupportedError;
    case xnn_status_out_of_memory:
      return DOMExceptionCode::kQuotaExceededError;
  }
}

// SharedXnnpackContext is shared and reference-counted by all MLGraphXnnpack
// instances. It initializes the XNNPACK library when the first MLGraphXnnpack
// calls SharedXnnpackContext::GetInstance(). It deinitializes the XNNPACK
// library (expect Linux/ChromeOS see comments below) when the last
// MLGraphXnnpack instance is garbage collected.
class SharedXnnpackContext : public ThreadSafeRefCounted<SharedXnnpackContext> {
 public:
  static scoped_refptr<SharedXnnpackContext> GetInstance(
      String& error_message) {
    base::AutoLock auto_lock(SharedXnnpackContextLock());
    if (instance_ == nullptr) {
      // Initializes XNNPACK library. By passing nullptr to allocator argument,
      // the XNNPACK default memory allocator will be used. The XNNPACK default
      // memory allocator uses system-provided memory management functions
      // (e.g., malloc()/_aligned_malloc()/free()). In Chromium build, these
      // functions are intercepted to PartitionAlloc.
      xnn_status status = xnn_initialize(nullptr);
      if (status != xnn_status_success) {
        error_message = "Failed to initialize the XNNPACK library: " +
                        XnnStatusToString(status);
        return nullptr;
      }

      // TODO(crbug.com/1273291): Integrate XNNPACK pthreadpool with
      // base::ThreadPool for performance optimziation on multi-cores in the
      // future.

      // Create a new instance of SharedXnnpackContext.
      return base::MakeRefCounted<SharedXnnpackContext>();
    } else {
      // Add a reference to the existing SharedXnnpackContext instance.
      return base::WrapRefCounted(instance_);
    }
  }

  SharedXnnpackContext(const SharedXnnpackContext&) = delete;
  SharedXnnpackContext& operator=(const SharedXnnpackContext&) = delete;

 private:
  friend class ThreadSafeRefCounted<SharedXnnpackContext>;
  template <typename T, typename... Args>
  friend scoped_refptr<T> base::MakeRefCounted(Args&&... args);

  explicit SharedXnnpackContext() { instance_ = this; }

  ~SharedXnnpackContext() {
    base::AutoLock auto_lock(SharedXnnpackContextLock());
#if !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
    // For Linux and ChromeOS, cpuinfo needs to parse /proc/cpuinfo to
    // initialize in pre sandbox stage. Calling xnn_deinitialize() here will
    // deinitialize cpuinfo within sandbox and cannot access /proc/cpuinfo
    // again.
    // See https://chromium-review.googlesource.com/c/chromium/src/+/3907965 for
    // more details.
    xnn_deinitialize();
#endif
    DCHECK_EQ(this, instance_);
    instance_ = nullptr;
  }

  static base::Lock& SharedXnnpackContextLock() {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(base::Lock, lock, ());
    return lock;
  }

  static SharedXnnpackContext* instance_ GUARDED_BY(SharedXnnpackContextLock());
};

SharedXnnpackContext* SharedXnnpackContext::instance_ = nullptr;

}  // namespace

// static
void MLGraphXnnpack::ValidateAndBuildAsync(MLContext* context,
                                           const MLNamedOperands& named_outputs,
                                           ScriptPromiseResolver* resolver) {
  auto* graph = MakeGarbageCollected<MLGraphXnnpack>(context);
  graph->BuildAsync(named_outputs, resolver);
}

// static
MLGraph* MLGraphXnnpack::ValidateAndBuildSync(
    MLContext* context,
    const MLNamedOperands& named_outputs,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<MLGraphXnnpack>(context)->BuildSync(
      named_outputs, exception_state);
}

MLGraphXnnpack::MLGraphXnnpack(MLContext* context) : MLGraph(context) {}

MLGraphXnnpack::~MLGraphXnnpack() = default;

// static
HeapVector<Member<const MLOperator>>*
MLGraphXnnpack::GetOperatorsInTopologicalOrder(
    const MLNamedOperands& named_outputs) {
  // A WebNN graph is represented by a directed acyclic graph (DAG) that has
  // operators as vertices and operand as edges. The topological sorting is
  // implemented by depth-first search (DFS) and visiting vertices in
  // post-order. It means a vertex (operator) is visited (pushed to the back of
  // the sorted list) after all its dependent vertices (operators) are visited.
  // With that, it ensures operator 'j' appears before operator 'i' in the
  // result, if 'i' depends on 'j'. The DFS algorithm is based on the
  // non-recursive implementation of:
  // https://en.wikipedia.org/wiki/Depth-first_search

  // The topologically sorted operators.
  auto* toposorted_operators =
      MakeGarbageCollected<HeapVector<Member<const MLOperator>>>();

  // The to-visit stack and visited set for DFS graph traversal.
  HeapDeque<Member<const MLOperator>> operators_to_visit;
  HeapHashSet<Member<const MLOperator>> visited_operators;
  // Enumerate output operands and initialize the to-visit stack with their
  // dependent operators.
  for (const auto& output : named_outputs) {
    const auto* operand = output.second.Get();
    operators_to_visit.push_back(operand->Operator());
  }
  while (operators_to_visit.size() > 0) {
    // Get the current operator from the top of the to-visit stack.
    const auto& current_operator = operators_to_visit.back();
    if (!visited_operators.Contains(current_operator.Get())) {
      // The current operator is not visited, check whether its dependent
      // operators are visited or not.
      bool skip_visit = false;
      for (const auto& operand : current_operator->Inputs()) {
        if (operand->Kind() == MLOperand::OperandKind::kOutput) {
          const auto* dependent_operator = operand->Operator();
          DCHECK(dependent_operator);
          if (!visited_operators.Contains(dependent_operator)) {
            // As there is an dependent operator is not visited, skip visiting
            // this operator and push the dependent operator into the to-visit
            // stack.
            skip_visit = true;
            operators_to_visit.push_back(dependent_operator);
          }
        }
      }
      if (!skip_visit) {
        // When all dependent operators have been visited, visit the current
        // operator and add it into the visited set.
        toposorted_operators->push_back(current_operator);
        visited_operators.insert(current_operator);
        // Pop the current operator from the to-visit stack.
        operators_to_visit.pop_back();
      }
    } else {
      // The current operator is already visited, pop it and check the next
      // one.
      operators_to_visit.pop_back();
    }
  }
  return toposorted_operators;
}

void MLGraphXnnpack::BuildAsyncImpl(const MLNamedOperands& named_outputs,
                                    ScriptPromiseResolver* resolver) {
  // TODO(crbug.com/1273291): Revisit whether the topological sorting should run
  // in the worker thread.
  auto* toposorted_operators = GetOperatorsInTopologicalOrder(named_outputs);
  // TODO(crbug.com/1273291): Get a dedicated queue when the specification
  // matures.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      ExecutionContext::From(resolver->GetScriptState())
          ->GetTaskRunner(TaskType::kMiscPlatformAPI);
  worker_pool::PostTask(
      FROM_HERE,
      CrossThreadBindOnce(
          &BuildOnBackgroundThread, WrapCrossThreadPersistent(this),
          WrapCrossThreadPersistent(toposorted_operators),
          WrapCrossThreadPersistent(resolver), std::move(task_runner)));
}

// static
void MLGraphXnnpack::BuildOnBackgroundThread(
    CrossThreadPersistent<MLGraphXnnpack> graph,
    CrossThreadPersistent<HeapVector<Member<const MLOperator>>>
        toposorted_operators,
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner) {
  DCHECK(!IsMainThread());
  DCHECK(!graph->xnn_context_);

  // Get or create the SharedXnnpackContext.
  String error_message;
  xnn_status status = xnn_status_success;
  graph->xnn_context_ = SharedXnnpackContext::GetInstance(error_message);
  if (!graph->xnn_context_) {
    status = xnn_status_uninitialized;
  }

  // TODO(ningxin.hu@intel.com): Define the XNNPACK subgraph Nodes for the
  // topologically sorted operators and subgraph Values for their input and
  // output operands.

  PostCrossThreadTask(*resolver_task_runner, FROM_HERE,
                      CrossThreadBindOnce(&MLGraphXnnpack::OnBuildFinished,
                                          std::move(graph), std::move(resolver),
                                          status, std::move(error_message)));
}

void MLGraphXnnpack::OnBuildFinished(
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    xnn_status status,
    String error_message) {
  if (status != xnn_status_success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        XnnStatusToDOMExceptionCode(status), error_message));
    return;
  }
  resolver->Resolve(this);
}

MLGraph* MLGraphXnnpack::BuildSyncImpl(const MLNamedOperands& named_outputs,
                                       ExceptionState& exception_state) {
  DCHECK(!xnn_context_);
  String error_message;
  xnn_context_ = SharedXnnpackContext::GetInstance(error_message);
  if (!xnn_context_) {
    exception_state.ThrowDOMException(
        XnnStatusToDOMExceptionCode(xnn_status_uninitialized), error_message);
    return nullptr;
  }

  // TODO(ningxin.hu@intel.com): Sort the operators in topological order. Define
  // the XNNPACK subgraph Nodes for the sorted operators and subgraph Values for
  // their input and output operands.

  return this;
}

void MLGraphXnnpack::ComputeAsyncImpl(const MLNamedArrayBufferViews& inputs,
                                      const MLNamedArrayBufferViews& outputs,
                                      ScriptPromiseResolver* resolver) {
  // TODO(ningxin.hu@intel.com): Implement this method by posting the inputs and
  // outputs to a background thread and invoking XNNPACK Runtime object in the
  // background thread.
  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError, "Not implemented."));
}

}  // namespace blink
