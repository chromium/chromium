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

MLGraphXnnpack::MLGraphXnnpack(MLContext* context) : MLGraph(context) {}

MLGraphXnnpack::~MLGraphXnnpack() = default;

void MLGraphXnnpack::BuildAsyncImpl(const MLNamedOperands& named_outputs,
                                    ScriptPromiseResolver* resolver) {
  // TODO(crbug.com/1273291): Get a dedicated queue when the specification
  // matures.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      ExecutionContext::From(resolver->GetScriptState())
          ->GetTaskRunner(TaskType::kMiscPlatformAPI);
  auto* on_heap_named_outputs =
      MakeGarbageCollected<MLNamedOperands>(named_outputs);
  worker_pool::PostTask(
      FROM_HERE,
      CrossThreadBindOnce(
          &BuildOnBackgroundThread, WrapCrossThreadPersistent(this),
          WrapCrossThreadPersistent(on_heap_named_outputs),
          WrapCrossThreadPersistent(resolver), std::move(task_runner)));
}

// static
void MLGraphXnnpack::BuildOnBackgroundThread(
    CrossThreadPersistent<MLGraphXnnpack> graph,
    CrossThreadPersistent<MLNamedOperands> named_outputs,
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

  // TODO(ningxin.hu@intel.com): Sort the operators topoloically by searching
  // from named_outputs, build an XNNPACK Subgraph object based those operators
  // and create an XNNPACK Runtime object for accelerated execution.

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
