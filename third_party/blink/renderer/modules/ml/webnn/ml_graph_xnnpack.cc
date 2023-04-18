// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_xnnpack.h"

#include <algorithm>

#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/trace_event/typed_macros.h"
#include "build/buildflag.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_compute_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_leaky_relu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pad_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_resample_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_transpose_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_activation.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
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

// Maps MLOperand pointer address to its XNNPACK Value ID.
//
// Use `const void*` here because this HashMap might be used in a worker thread
// that doesn't support GC.
//
// This map is only used in CreateXnnSubgraph(), who owns references to
// MLOperands, so it's safe to use raw pointers here.
//
// TODO(crbug.com/1273291): Consider getting GC support in worker threads, so
// the safer `HeapHashMap<Member<MLOperand>, uint32_t>` could be used instead.
using OperandValueIdMap = HashMap<const void*, uint32_t>;

#define XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(xnn_func)            \
  {                                                                 \
    xnn_status status = xnn_func;                                   \
    if (status != xnn_status_success) {                             \
      error_message =                                               \
          String::Format("Failed to call %s: %s.", #xnn_func,       \
                         XnnStatusToString(status).Utf8().c_str()); \
      return status;                                                \
    }                                                               \
  }

#define XNN_CHECK_STATUS(xnn_func)      \
  {                                     \
    xnn_status status = xnn_func;       \
    if (status != xnn_status_success) { \
      return status;                    \
    }                                   \
  }

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

String XnnDataTypeToString(xnn_datatype datatype) {
  switch (datatype) {
    case xnn_datatype_invalid:
      return "xnn_datatype_invalid";
    case xnn_datatype_fp32:
      return "xnn_datatype_fp32";
    case xnn_datatype_fp16:
      return "xnn_datatype_fp16";
    case xnn_datatype_qint8:
      return "xnn_datatype_qint8";
    case xnn_datatype_quint8:
      return "xnn_datatype_quint8";
    case xnn_datatype_qint32:
      return "xnn_datatype_qint32";
    case xnn_datatype_qcint8:
      return "xnn_datatype_qcint8";
    case xnn_datatype_qcint32:
      return "xnn_datatype_qcint32";
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
    TRACE_EVENT("blink", "SharedXnnpackContext::GetInstance");
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

// `XnnRuntimeWrapper` retains objects that aren't managed by Blink GC.
class XnnRuntimeWrapper : public ThreadSafeRefCounted<XnnRuntimeWrapper> {
 public:
  ~XnnRuntimeWrapper() {
    // Explicitly destroy XNNPACK Runtime before releasing static data buffers.
    // It ensures the lifetime of static data buffers exceeds the lifetime of
    // this Runtime object.
    xnn_runtime_.reset();
    static_data_buffers_.clear();
  }

  XnnRuntimeWrapper(const XnnRuntimeWrapper&) = delete;
  XnnRuntimeWrapper& operator=(const XnnRuntimeWrapper&) = delete;

  const Vector<xnn_external_value>& GetXnnExternalValuesTesting() const {
    return *xnn_external_values_;
  }

  // Creates an XNNPACK Runtime object from the Subgraph object. The Runtime
  // object is a combination of an execution plan for Subgraph Nodes and a
  // memory manager for Subgraph Values and will be used for the accelerated
  // executions. This method can run either in a background thread for
  // asynchronous graph building or in the caller's thread for synchronous graph
  // building.
  static scoped_refptr<XnnRuntimeWrapper> Create(
      XnnSubgraphPtr subgraph,
      scoped_refptr<SharedXnnpackContext> xnn_context,
      Vector<DataBufferPtr> static_data_buffers,
      String& error_message) {
    TRACE_EVENT("blink", "XnnRuntimeWrapper::Create");
    CHECK(xnn_context);
    CHECK(subgraph);
    xnn_runtime_t runtime_ptr = nullptr;
    xnn_status status = xnn_create_runtime(subgraph.get(), &runtime_ptr);
    if (status != xnn_status_success) {
      error_message = "Failed to create XNNPACK Runtime.";
      return nullptr;
    }
    CHECK(runtime_ptr);
    return base::MakeRefCounted<XnnRuntimeWrapper>(
        runtime_ptr, std::move(xnn_context), std::move(static_data_buffers));
  }

  // Invoke the XNNPACK Runtime object. If there are any data pointers changed,
  // setup the XNNPACK Runtime with the updated `xnn_external_values` before the
  // invocation.
  xnn_status Invoke(XnnExternalValuesPtr external_values,
                    String& error_message) {
    TRACE_EVENT("blink", "XnnRuntimeWrapper::Invoke");
    CHECK(external_values);

    // Check if any data pointers of the provided `xnn_external_values` changed
    // against the pointers that has been setup (stored in
    // `xnn_external_values_`).
    //
    // The change may be caused by user providing a different ArrayBufferView
    // that is backed by a newly allocated or reallocated store.
    //
    // The XNNPACK Runtime object setup may be expensive. If the data pointers
    // haven't changed, there's no need to redo the setup.
    bool need_to_setup_external_values =
        !base::ranges::equal(*external_values, *xnn_external_values_,
                             [](const auto& a, const auto& b) {
                               return a.id == b.id && a.data == b.data;
                             });

    if (need_to_setup_external_values) {
      XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(
          xnn_setup_runtime(xnn_runtime_.get(), external_values->size(),
                            external_values->data()));
      xnn_external_values_ = std::move(external_values);
    }

    XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(
        xnn_invoke_runtime(xnn_runtime_.get()));
    return xnn_status_success;
  }

 private:
  friend class ThreadSafeRefCounted<XnnRuntimeWrapper>;

  template <typename T, typename... Args>
  friend scoped_refptr<T> base::MakeRefCounted(Args&&... args);

  XnnRuntimeWrapper(xnn_runtime_t xnn_runtime,
                    scoped_refptr<SharedXnnpackContext> xnn_context,
                    Vector<DataBufferPtr> static_data_buffers)
      : xnn_context_(std::move(xnn_context)),
        static_data_buffers_(std::move(static_data_buffers)),
        xnn_external_values_(std::make_unique<Vector<xnn_external_value>>()),
        xnn_runtime_({xnn_runtime, &xnn_delete_runtime}) {}

  // The SharedXnnpackContext is shared and reference-counted by all instances
  // of MLGraphXnnpack. It initializes (and also deinitializes) the XNNPACK
  // library for graph building and execution.
  scoped_refptr<SharedXnnpackContext> xnn_context_;

  // Holds the static data of XNNPACK Values for MLGraph's constant operands.
  // The data must outlive XNNPACK Subgraph and Runtime objects using them.
  Vector<DataBufferPtr> static_data_buffers_;

  // Holds the XNNPACK external values (value ID and data pointer) used for
  // Runtime setup. It is used to avoid unnecessary Runtime setup if no pointers
  // are changed. See more details in the comment of the `Invoke()` method.
  XnnExternalValuesPtr xnn_external_values_;

  // The XNNPACK Runtime object for the accelerated executions.
  std::unique_ptr<xnn_runtime, decltype(&xnn_delete_runtime)> xnn_runtime_;
};

// Stores information about a transferred `ArrayBufferView`. This struct doesn't
// include Blink GC objects, and can be accessed by any threads.
//
// The information is used to recreate `ArrayBufferView` when computation
// completes.
struct ArrayBufferViewInfo {
  ArrayBufferViewInfo() = default;
  ~ArrayBufferViewInfo() = default;

  ArrayBufferViewInfo(ArrayBufferViewInfo&& other) = default;
  ArrayBufferViewInfo& operator=(ArrayBufferViewInfo&& other) = default;

  ArrayBufferViewInfo(const ArrayBufferViewInfo&) = delete;
  ArrayBufferViewInfo& operator=(const ArrayBufferViewInfo&) = delete;

  DOMArrayBufferView::ViewType type;
  size_t offset;
  size_t length;
  ArrayBufferContents contents;
};

absl::optional<ArrayBufferViewInfo> TransferArrayBufferView(
    v8::Isolate* isolate,
    NotShared<DOMArrayBufferView> source_view,
    ExceptionState& exception_state) {
  // A detached ArrayBufferView should be caught by
  // `ValidateNamedArrayBufferViews()` called in `MLGraph::ComputeAsync()`.
  CHECK(!source_view->IsDetached());

  // Avoid transferring a non-detachable ArrayBuffer.
  // `DOMArrayBuffer::Transfer()` would make a copy if the ArrayBuffer is not
  // detachable. This behavior doesn't follow the algorithm to transfer an
  // ArrayBuffer of WebIDL spec:
  // https://webidl.spec.whatwg.org/#arraybuffer-transfer
  if (!source_view->buffer()->IsDetachable(isolate)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The ArrayBuffer is not detachable.");
    return absl::nullopt;
  }

  // Get the offset and length of the source view before transferring it.
  ArrayBufferViewInfo view_info;
  view_info.type = source_view->GetType();
  view_info.offset = source_view->byteOffset();
  view_info.length = source_view->byteLength() / source_view->TypeSize();

  ArrayBufferContents contents;
  // The following `DOMArrayBuffer::Transfer()` call would fail if the
  // detach key of the ArrayBuffer is not `undefined`.
  if (!source_view->buffer()->Transfer(isolate, view_info.contents,
                                       exception_state)) {
    return absl::nullopt;
  }

  return view_info;
}

DOMArrayBufferView* CreateArrayBufferView(ArrayBufferViewInfo view_info) {
  auto* target_buffer = DOMArrayBuffer::Create(std::move(view_info.contents));

  // Align with the ArrayBufferView types supported by WebNN MLOperandType:
  // https://www.w3.org/TR/webnn/#appendices-mloperandtype-arraybufferview-compatibility
  DOMArrayBufferView* target_view = nullptr;
  switch (view_info.type) {
    case DOMArrayBufferView::kTypeFloat32:
      // Float32Array is used for MLOperandType::float32.
      target_view = DOMFloat32Array::Create(target_buffer, view_info.offset,
                                            view_info.length);
      break;
    case DOMArrayBufferView::kTypeUint16:
      // Using Uint16Array for float16 is a workaround of WebNN spec issue:
      // https://github.com/webmachinelearning/webnn/issues/127
      target_view = DOMUint16Array::Create(target_buffer, view_info.offset,
                                           view_info.length);
      break;
    case DOMArrayBufferView::kTypeInt32:
      // Int32Array is used for MLOperandType::int32.
      target_view = DOMInt32Array::Create(target_buffer, view_info.offset,
                                          view_info.length);
      break;
    case DOMArrayBufferView::kTypeUint32:
      // Uint32Array is used for MLOperandType::uint32.
      target_view = DOMUint32Array::Create(target_buffer, view_info.offset,
                                           view_info.length);
      break;
    case DOMArrayBufferView::kTypeInt8:
      // Int8Array is used for MLOperandType::int8.
      target_view = DOMInt8Array::Create(target_buffer, view_info.offset,
                                         view_info.length);
      break;
    case DOMArrayBufferView::kTypeUint8:
      // Uint8Array is used for MLOperandType::uint8.
      target_view = DOMUint8Array::Create(target_buffer, view_info.offset,
                                          view_info.length);
      break;
    default:
      // Other ArrayBufferView types should not pass the
      // `ValidateNamedArrayBufferViews()` and reach here.
      NOTREACHED_NORETURN();
  }
  return target_view;
}

// `TransferNamedArrayBufferViews()` and `CreateNamedArrayBufferViews()`
// implement the MLNamedArrayBufferViews transfer algorithm of WebNN spec:
// https://www.w3.org/TR/webnn/#mlnamedarraybufferviews-transfer
//
// The `NamedArrayBufferViewsInfo` returned by `TransferNamedArrayBufferViews()`
// doesn't contain any GC objects, so it is safe to be posted to a background
// thread that invokes the XNNPACK Runtime. After that,
// `NamedArrayBufferViewsInfo` should be posted back to the calling thread and
// call `CreateNamedArrayBufferViews()` to create `MLNamedArrayBufferViews` from
// the info.
//
// If it fails to transfer an `ArrayBufferView` of the
// `MLNamedArrayBufferViews`, the current implementation leaves the
// already-transferred views detached, the failing one and remaining others
// unchanged.
//
// TODO(crbug.com/1273291): Revisit the error handling once the WebNN spec issue
// is resolved: https://github.com/webmachinelearning/webnn/issues/351
NamedArrayBufferViewsInfoPtr TransferNamedArrayBufferViews(
    v8::Isolate* isolate,
    const MLNamedArrayBufferViews& source_views,
    ExceptionState& exception_state) {
  auto views_info = std::make_unique<NamedArrayBufferViewsInfo>();
  for (const auto& [name, source_view] : source_views) {
    auto view_info =
        TransferArrayBufferView(isolate, source_view, exception_state);
    if (!view_info) {
      return nullptr;
    }
    views_info->push_back(std::make_pair(name, std::move(view_info.value())));
  }
  return views_info;
}

MLNamedArrayBufferViews* CreateNamedArrayBufferViews(
    NamedArrayBufferViewsInfoPtr views_info) {
  auto* target_views = MakeGarbageCollected<MLNamedArrayBufferViews>();
  for (auto& [name, view_info] : *views_info) {
    target_views->push_back(
        std::make_pair(name, CreateArrayBufferView(std::move(view_info))));
  }
  return target_views;
}

xnn_datatype GetXnnDataType(V8MLOperandType::Enum operand_type) {
  switch (operand_type) {
    case V8MLOperandType::Enum::kFloat32:
      return xnn_datatype_fp32;
    case V8MLOperandType::Enum::kFloat16:
      return xnn_datatype_fp16;
    case V8MLOperandType::Enum::kInt32:
    case V8MLOperandType::Enum::kUint32:
    case V8MLOperandType::Enum::kInt8:
    case V8MLOperandType::Enum::kUint8:
      // TODO(crbug.com/1273291): Support the quantized integer types that is a
      // WebNN v2 feature tracked by:
      // https://github.com/webmachinelearning/webnn/issues/128.
      return xnn_datatype_invalid;
  }
}

Vector<size_t> GetXnnDimensions(const Vector<uint32_t>& operand_dimensions) {
  Vector<size_t> xnn_dimensions;
  for (const auto d : operand_dimensions) {
    xnn_dimensions.push_back(base::checked_cast<size_t>(d));
  }
  return xnn_dimensions;
}

// DefineXnnValue() defines an XNNPACK Value for a WebNN operand. If there are
// no errors, it returns xnn_status_success and the value_id is set to the
// XNNPACK Value's ID.
//
// This method should not be used directly. Please use the specialized
// DefineExternalXnnValue(), DefineInternalXnnValue() and DefineStaticXnnValue()
// methods instead.
//
// If the data pointer is not nullptr, it is safe to be used to initialize the
// XNNPACK Value. Because its buffer is holded by this MLGraph object's
// static_data_buffers_ member, it would outlive the XNNPACK Value who uses it.
xnn_status DefineXnnValue(xnn_subgraph_t subgraph,
                          const MLOperand* operand,
                          const DataBufferPtr& data,
                          uint32_t external_value_id,
                          uint32_t& value_id,
                          String& error_message) {
  DCHECK(operand);
  xnn_datatype datatype = GetXnnDataType(operand->Type());
  if (datatype == xnn_datatype_invalid) {
    error_message = "The operand type (" +
                    V8MLOperandType(operand->Type()).AsString() +
                    ") is not supported.";
    return xnn_status_unsupported_parameter;
  }
  Vector<size_t> dims = GetXnnDimensions(operand->Dimensions());

  uint32_t flags = 0;
  if (external_value_id != XNN_INVALID_VALUE_ID) {
    // External Values should not be initialized with static data.
    DCHECK(!data);
    switch (operand->Kind()) {
      case MLOperand::OperandKind::kInput:
        flags = XNN_VALUE_FLAG_EXTERNAL_INPUT;
        break;
      case MLOperand::OperandKind::kOutput:
        flags = XNN_VALUE_FLAG_EXTERNAL_OUTPUT;
        break;
      case MLOperand::OperandKind::kConstant:
        // Should not define an external Value for constant operand.
        NOTREACHED();
        break;
    }
  }

  switch (datatype) {
    case xnn_datatype_fp32:
    case xnn_datatype_fp16:
      XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(xnn_define_tensor_value(
          subgraph, datatype, dims.size(), dims.data(), data.get(),
          external_value_id, flags, &value_id));
      break;
    default:
      // TODO(crbug.com/1273291): Call xnn_define_quantized_tensor_value() once
      // WebNN supports quantized integer types that is tracked by
      // https://github.com/webmachinelearning/webnn/issues/128
      error_message = "The data type (" + XnnDataTypeToString(datatype) +
                      ") is not supported.";
      return xnn_status_unsupported_parameter;
  }

  return xnn_status_success;
}

// Define an external XNNPACK Value given a WebNN graph's input or output
// operand.
xnn_status DefineExternalXnnValue(xnn_subgraph_t subgraph,
                                  const MLOperand* operand,
                                  uint32_t external_value_id,
                                  uint32_t& value_id,
                                  String& error_message) {
  DCHECK_NE(external_value_id, XNN_INVALID_VALUE_ID);
  return DefineXnnValue(subgraph, operand, DataBufferPtr(nullptr),
                        external_value_id, value_id, error_message);
}

// Define an internal XNNPACK Value given a WebNN graph's intermediate
// operand that connects with two operators.
xnn_status DefineInternalXnnValue(xnn_subgraph_t subgraph,
                                  const MLOperand* operand,
                                  uint32_t& value_id,
                                  String& error_message) {
  // Set external_value_id to XNN_INVALID_VALUE_ID, so an internal ID will be
  // created for the Value and value_id will be set to that internal ID.
  return DefineXnnValue(subgraph, operand, DataBufferPtr(nullptr),
                        XNN_INVALID_VALUE_ID, value_id, error_message);
}

// Define a static XNNPACK Value given a WebNN graph's constant operand and its
// data. XNNPACK requires the life-time of the data must exceed the life-time of
// the Subgraph object, and of any Runtime objects created from the Subgraph.
xnn_status DefineStaticXnnValue(xnn_subgraph_t subgraph,
                                const MLOperand* operand,
                                const DataBufferPtr& data,
                                uint32_t& value_id,
                                String& error_message) {
  DCHECK(data);
  // Set external_value_id to XNN_INVALID_VALUE_ID, so an internal ID will be
  // created for the Value and value_id will be set to that internal ID.
  return DefineXnnValue(subgraph, operand, data, XNN_INVALID_VALUE_ID, value_id,
                        error_message);
}

uint32_t GetOperatorInputValueId(const MLOperator* op,
                                 const OperandValueIdMap& operand_value_id_map,
                                 wtf_size_t index = 0) {
  DCHECK_LE(index, op->Inputs().size());
  const auto* input = op->Inputs()[index].Get();
  DCHECK_NE(op, nullptr);
  DCHECK(operand_value_id_map.Contains(input));
  return operand_value_id_map.at(input);
}

uint32_t GetOperatorOutputValueId(const MLOperator* op,
                                  const OperandValueIdMap& operand_value_id_map,
                                  wtf_size_t index = 0) {
  DCHECK_LE(index, op->Outputs().size());
  const auto* output = op->Outputs()[index].Get();
  DCHECK_NE(op, nullptr);
  DCHECK(operand_value_id_map.Contains(output));
  return operand_value_id_map.at(output);
}

struct XnnOutputRange {
  float min;
  float max;
};

// Helper to get XNNPACK Node output value range for WebNN activation operators.
XnnOutputRange GetXnnOutputRangeForActivation(const MLOperator* ml_operator) {
  DCHECK(ml_operator);
  XnnOutputRange output_range;
  switch (ml_operator->Kind()) {
    // TODO(crbug.com/1273291): Support clamp.
    case MLOperator::OperatorKind::kClamp: {
      // According to WebNN clamp spec:
      // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-clamp, clamping occurs
      // only if the lower bound or/and upper bound are provided.
      const auto* options =
          static_cast<const MLClampOptions*>(ml_operator->Options());
      DCHECK(options);
      output_range.min =
          options->getMinValueOr(-std::numeric_limits<float>::infinity());
      output_range.max =
          options->getMaxValueOr(+std::numeric_limits<float>::infinity());
      break;
    }
    case MLOperator::OperatorKind::kRelu:
      // Set the minimum value to 0 according to the rectified linear function,
      // y = max(0, x).
      output_range.min = 0.0f;
      output_range.max = +std::numeric_limits<float>::infinity();
      break;
    default:
      // Only clamp and relu are supported.
      NOTREACHED();
  }
  return output_range;
}

xnn_status DefineXnnNodeForClamp(xnn_subgraph_t subgraph,
                                 const MLOperator* clamp,
                                 const OperandValueIdMap& operand_value_id_map,
                                 String& error_message) {
  const uint32_t input_id =
      GetOperatorInputValueId(clamp, operand_value_id_map);
  const uint32_t output_id =
      GetOperatorOutputValueId(clamp, operand_value_id_map);
  const auto output_range = GetXnnOutputRangeForActivation(clamp);
  const uint32_t flags = 0;
  XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(
      xnn_define_clamp(subgraph, output_range.min, output_range.max, input_id,
                       output_id, flags));
  return xnn_status_success;
}

struct XnnPadding2D {
  uint32_t top;
  uint32_t bottom;
  uint32_t left;
  uint32_t right;
};

// Helper to get padding sizes for XNNPACK convolution 2d or pooling 2d Nodes.
template <typename OptionsType>
XnnPadding2D GetXnnPadding2D(const OptionsType* options,
                             uint32_t input_height,
                             uint32_t input_width,
                             uint32_t filter_height,
                             uint32_t filter_width,
                             uint32_t stride_height,
                             uint32_t stride_width,
                             uint32_t dilation_height,
                             uint32_t dilation_width) {
  XnnPadding2D xnn_padding;
  switch (options->autoPad().AsEnum()) {
    case V8MLAutoPad::Enum::kExplicit: {
      // Set the XNNPACK padding from WebNN explicit padding that is in
      // [beginning_height, ending_height, beginning_width, ending_width],
      // default to 0.
      const Vector<uint32_t> default_pads({0, 0, 0, 0});
      xnn_padding.top = options->getPaddingOr(default_pads)[0];
      xnn_padding.bottom = options->getPaddingOr(default_pads)[1];
      xnn_padding.left = options->getPaddingOr(default_pads)[2];
      xnn_padding.right = options->getPaddingOr(default_pads)[3];
      break;
    }
    case V8MLAutoPad::Enum::kSameUpper:
    case V8MLAutoPad::Enum::kSameLower: {
      // Calculate the XNNPACK padding based on WebNN auto padding mode and
      // sizes.
      auto padding_sizes_height = MLGraphBuilder::CalculateConv2dPadding(
          options->autoPad().AsEnum(), input_height, filter_height,
          stride_height, dilation_height);
      DCHECK(padding_sizes_height);
      xnn_padding.top = padding_sizes_height.value().begin;
      xnn_padding.bottom = padding_sizes_height.value().end;
      auto padding_sizes_width = MLGraphBuilder::CalculateConv2dPadding(
          options->autoPad().AsEnum(), input_width, filter_width, stride_width,
          dilation_width);
      xnn_padding.left = padding_sizes_width.value().begin;
      xnn_padding.right = padding_sizes_width.value().end;
      break;
    }
  }
  return xnn_padding;
}

xnn_status DefineXnnNodeForConv2d(xnn_subgraph_t subgraph,
                                  const MLOperator* conv2d,
                                  const OperandValueIdMap& operand_value_id_map,
                                  String& error_message) {
  const uint32_t input_id =
      GetOperatorInputValueId(conv2d, operand_value_id_map, 0);
  const uint32_t filter_id =
      GetOperatorInputValueId(conv2d, operand_value_id_map, 1);
  // If there is no bias operand, set the XNNPACK Value ID of bias tensor to
  // XNN_INVALID_VALUE_ID.
  const uint32_t bias_id =
      conv2d->Inputs().size() == 3
          ? GetOperatorInputValueId(conv2d, operand_value_id_map, 2)
          : XNN_INVALID_VALUE_ID;
  const uint32_t output_id =
      GetOperatorOutputValueId(conv2d, operand_value_id_map);

  const MLConv2dOptions* options =
      static_cast<const MLConv2dOptions*>(conv2d->Options());

  // Set strides of XNNPACK conv2d, default to 1.
  const Vector<uint32_t> default_strides({1, 1});
  const uint32_t stride_height = options->getStridesOr(default_strides)[0];
  const uint32_t stride_width = options->getStridesOr(default_strides)[1];

  // Set dilations of XNNPACK conv2d, default to 1.
  const Vector<uint32_t> default_dilations({1, 1});
  const uint32_t dilation_height =
      options->getDilationsOr(default_dilations)[0];
  const uint32_t dilation_width = options->getDilationsOr(default_dilations)[1];

  // Set input and filter sizes of XNNPACK conv2d.
  uint32_t input_height, input_width;
  uint32_t filter_height, filter_width;
  uint32_t input_channels, output_channels;
  const uint32_t groups = options->groups();
  bool depthwise = false;
  if (options->inputLayout().AsEnum() == V8MLInputOperandLayout::Enum::kNhwc) {
    const auto* input = conv2d->Inputs()[0].Get();
    DCHECK(input);
    input_height = input->Dimensions()[1];
    input_width = input->Dimensions()[2];
    input_channels = input->Dimensions()[3];
    const auto* output = conv2d->Outputs()[0].Get();
    DCHECK(output);
    output_channels = output->Dimensions()[3];

    // According to WebNN conv2d spec:
    // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-conv2d, A depthwise
    // conv2d operation is a variant of grouped convolution where the
    // options.groups == input_channels == output_channels.
    depthwise =
        (groups == input_channels && groups == output_channels && groups != 1);
    if (!depthwise) {
      // For regular conv2d, XNNPACK expects weights layout in ohwi that is
      // [groups * group_output_channels, kernel_height, kernel_width,
      //  group_input_channels].
      //
      // TODO(crbug.com/1273291): support other layouts by transposing the
      // filter operand.
      if (options->filterLayout().AsEnum() !=
          V8MLConv2dFilterOperandLayout::Enum::kOhwi) {
        error_message = String::Format("The filter layout %s is not supported.",
                                       options->filterLayout().AsCStr());
        return xnn_status_unsupported_parameter;
      }
    } else {
      // For depthwise conv2d, XNNPACK expects weights layout in ihwo that is
      // [1, kernel_height, kernel_width, input_channels * depth_multiplier].
      //
      // TODO(crbug.com/1273291): support other layouts by transposing the
      // filter operand.
      if (options->filterLayout().AsEnum() !=
          V8MLConv2dFilterOperandLayout::Enum::kIhwo) {
        error_message = String::Format("The filter layout %s is not supported.",
                                       options->filterLayout().AsCStr());
        return xnn_status_unsupported_parameter;
      }
    }
    const auto* filter = conv2d->Inputs()[1].Get();
    DCHECK(filter);
    filter_height = filter->Dimensions()[1];
    filter_width = filter->Dimensions()[2];
  } else {
    // TODO(crbug.com/1273291): support other layouts by transposing the input
    // operand.
    error_message = String::Format("The input layout %s is not supported.",
                                   options->inputLayout().AsCStr());
    return xnn_status_unsupported_parameter;
  }

  // Set or calculate padding sizes of XNNPACK conv2d.
  const auto padding = GetXnnPadding2D(
      options, input_height, input_width, filter_height, filter_width,
      stride_height, stride_width, dilation_height, dilation_width);

  // Set the minimum and maximum output values for XNNPACK conv2d based on the
  // fused activation function. If no fused activation function is set, there
  // are no limits for output values.
  XnnOutputRange output_range{.min = -std::numeric_limits<float>::infinity(),
                              .max = +std::numeric_limits<float>::infinity()};
  if (options->hasActivation()) {
    switch (options->activation()->Operator()->Kind()) {
      case MLOperator::OperatorKind::kClamp:
      case MLOperator::OperatorKind::kRelu:
        output_range =
            GetXnnOutputRangeForActivation(options->activation()->Operator());
        break;
      default:
        error_message = "The fused operator (" +
                        MLOperator::OperatorKindToString(
                            options->activation()->Operator()->Kind()) +
                        ") is not supported by conv2d.";
        return xnn_status_unsupported_parameter;
    }
  }

  // Set group input and output channels of XNNPACK conv2d.
  const size_t group_input_channels = input_channels / groups;
  const size_t group_output_channels = output_channels / groups;

  // Define XNNPACK conv2d or depthwise conv2d Node for the Subgraph object.
  const uint32_t flags = 0;
  if (depthwise) {
    const uint32_t depth_multiplier = 1;
    XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(xnn_define_depthwise_convolution_2d(
        subgraph, padding.top, padding.right, padding.bottom, padding.left,
        filter_height, filter_width, stride_height, stride_width,
        dilation_height, dilation_width, depth_multiplier, input_channels,
        output_range.min, output_range.max, input_id, filter_id, bias_id,
        output_id, flags));
  } else {
    XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(xnn_define_convolution_2d(
        subgraph, padding.top, padding.right, padding.bottom, padding.left,
        filter_height, filter_width, stride_height, stride_width,
        dilation_height, dilation_width, groups, group_input_channels,
        group_output_channels, output_range.min, output_range.max, input_id,
        filter_id, bias_id, output_id, flags));
  }
  return xnn_status_success;
}

xnn_status DefineXnnNodeForElementWiseBinary(
    xnn_subgraph_t subgraph,
    const MLOperator* binary,
    const OperandValueIdMap& operand_value_id_map,
    String& error_message) {
  const uint32_t lhs_id =
      GetOperatorInputValueId(binary, operand_value_id_map, 0);
  const uint32_t rhs_id =
      GetOperatorInputValueId(binary, operand_value_id_map, 1);
  const uint32_t output_id =
      GetOperatorOutputValueId(binary, operand_value_id_map);
  const float output_min = -std::numeric_limits<float>::infinity();
  const float output_max = +std::numeric_limits<float>::infinity();
  const uint32_t flags = 0;
  switch (binary->Kind()) {
    case MLOperator::OperatorKind::kAdd: {
      XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(xnn_define_add2(
          subgraph, output_min, output_max, lhs_id, rhs_id, output_id, flags));
      break;
    }
    case MLOperator::OperatorKind::kSub: {
      XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(xnn_define_subtract(
          subgraph, output_min, output_max, lhs_id, rhs_id, output_id, flags));
      break;
    }
    case MLOperator::OperatorKind::kMul: {
      XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(xnn_define_multiply2(
          subgraph, output_min, output_max, lhs_id, rhs_id, output_id, flags));
      break;
    }
    case MLOperator::OperatorKind::kDiv: {
      XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(xnn_define_divide(
          subgraph, output_min, output_max, lhs_id, rhs_id, output_id, flags));
      break;
    }
    case MLOperator::OperatorKind::kMax: {
      XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(
          xnn_define_maximum2(subgraph, lhs_id, rhs_id, output_id, flags));
      break;
    }
    case MLOperator::OperatorKind::kMin: {
      XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(
          xnn_define_minimum2(subgraph, lhs_id, rhs_id, output_id, flags));
      break;
    }
    default:
      NOTREACHED();
  }
  return xnn_status_success;
}

xnn_status DefineXnnNodeForGemm(xnn_subgraph_t subgraph,
                                const MLOperator* gemm,
                                const OperandValueIdMap& operand_value_id_map,
                                String& error_message) {
  // Set up the Value ID of input, filter, bias and output tensors for XNNPACK
  // fully connected Node.
  const uint32_t input_id =
      GetOperatorInputValueId(gemm, operand_value_id_map, 0);
  const uint32_t filter_id =
      GetOperatorInputValueId(gemm, operand_value_id_map, 1);
  // Set the Value ID of bias tensor to XNN_INVALID_VALUE_ID if it is not
  // present.
  const uint32_t bias_id =
      gemm->Inputs().size() == 3
          ? GetOperatorInputValueId(gemm, operand_value_id_map, 2)
          : XNN_INVALID_VALUE_ID;
  const uint32_t output_id =
      GetOperatorOutputValueId(gemm, operand_value_id_map);

  const MLGemmOptions* options =
      static_cast<const MLGemmOptions*>(gemm->Options());
  if (options->hasC()) {
    // XNNPACK fully connected Node only supports 1-D bias tensor (operand c of
    // WebNN gemm operator) with [output_channels] dimensions.
    const auto* bias = options->c();
    const auto output_channels = gemm->Outputs()[0]->Dimensions()[1];
    if (bias->Dimensions().size() != 1u ||
        bias->Dimensions()[0] != output_channels) {
      // TODO(crbug.com/1273291): Support the bias with other dimensions by
      // element-wise addition operator.
      error_message = String::Format("The dimensions of bias must be [%u].",
                                     output_channels);
      return xnn_status_unsupported_parameter;
    }
  }
  if (fabs(options->alpha() - 1.0f) > std::numeric_limits<float>::epsilon()) {
    // TODO(crbug.com/1273291): Support alpha by using element-wise
    // multiplication operator.
    error_message = "gemm doesn't support alpha option.";
    return xnn_status_unsupported_parameter;
  }
  if (fabs(options->beta() - 1.0f) > std::numeric_limits<float>::epsilon()) {
    // TODO(crbug.com/1273291): Support beta by using element-wise
    // multiplication operator.
    error_message = "gemm doesn't support beta option.";
    return xnn_status_unsupported_parameter;
  }
  if (options->aTranspose()) {
    // TODO(crbug.com/1273291): Support aTranspose by using transpose operator.
    error_message = "gemm doesn't support aTranspose option.";
    return xnn_status_unsupported_parameter;
  }
  uint32_t flags = 0;
  if (!options->bTranspose()) {
    // When bTranspose option is false, the filter tensor (operand b of WebNN
    // gemm operator) has [input_channels, output_channels] dimensions that
    // requires the XNN_FLAG_TRANSPOSE_WEIGHTS flag to be set for XNNPACK fully
    // connected Node.
    flags = XNN_FLAG_TRANSPOSE_WEIGHTS;
  }
  const float output_min = -std::numeric_limits<float>::infinity();
  const float output_max = +std::numeric_limits<float>::infinity();
  XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(
      xnn_define_fully_connected(subgraph, output_min, output_max, input_id,
                                 filter_id, bias_id, output_id, flags));
  return xnn_status_success;
}

xnn_status DefineXnnNodeForHardSwish(
    xnn_subgraph_t subgraph,
    const MLOperator* hardswish,
    const OperandValueIdMap& operand_value_id_map,
    String& error_message) {
  const uint32_t input_id =
      GetOperatorInputValueId(hardswish, operand_value_id_map);
  const uint32_t output_id =
      GetOperatorOutputValueId(hardswish, operand_value_id_map);
  const uint32_t flags = 0;
  XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(
      xnn_define_hardswish(subgraph, input_id, output_id, flags));
  return xnn_status_success;
}

xnn_status DefineXnnNodeForLeakyRelu(
    xnn_subgraph_t subgraph,
    const MLOperator* leaky_relu,
    const OperandValueIdMap& operand_value_id_map,
    String& error_message) {
  const uint32_t input_id =
      GetOperatorInputValueId(leaky_relu, operand_value_id_map);
  const uint32_t output_id =
      GetOperatorOutputValueId(leaky_relu, operand_value_id_map);
  const MLLeakyReluOptions* options =
      static_cast<const MLLeakyReluOptions*>(leaky_relu->Options());
  CHECK(options);
  const float negative_slope = options->alpha();
  const uint32_t flags = 0;
  XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(xnn_define_leaky_relu(
      subgraph, negative_slope, input_id, output_id, flags));
  return xnn_status_success;
}

xnn_status DefineXnnNodeForPad(xnn_subgraph_t subgraph,
                               const MLOperator* pad,
                               const OperandValueIdMap& operand_value_id_map,
                               String& error_message) {
  const MLPadOperator* pad_operator = static_cast<const MLPadOperator*>(pad);
  const uint32_t input_id =
      GetOperatorInputValueId(pad_operator, operand_value_id_map);
  const uint32_t output_id =
      GetOperatorOutputValueId(pad_operator, operand_value_id_map);
  const MLPadOptions* options =
      static_cast<const MLPadOptions*>(pad_operator->Options());
  CHECK(options);
  if (options->mode() != V8MLPaddingMode::Enum::kConstant) {
    error_message = "XNNPACK only supports constant padding mode.";
    return xnn_status_unsupported_parameter;
  }

  const Vector<uint32_t> beginning_padding = pad_operator->BeginningPadding();
  Vector<size_t> pre_paddings(beginning_padding.size());
  base::ranges::transform(
      beginning_padding, pre_paddings.begin(),
      [](uint32_t p) { return base::checked_cast<size_t>(p); });
  const Vector<uint32_t> ending_padding = pad_operator->EndingPadding();
  Vector<size_t> post_paddings(ending_padding.size());
  base::ranges::transform(
      ending_padding, post_paddings.begin(),
      [](uint32_t p) { return base::checked_cast<size_t>(p); });

  float padding_value = options->value();
  const uint32_t flags = 0;
  // XNNPACK will memcpy the content of `pre_paddings` and `post_paddings`
  // vectors to its internal structure, so it is safe to release `pre_paddings`
  // and `post_padding` vectors after this call. Please refer to the
  // implementation at:
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/xnnpack/src/src/subgraph/static-constant-pad.c;l=245
  XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(xnn_define_static_constant_pad(
      subgraph, pre_paddings.data(), post_paddings.data(), padding_value,
      input_id, output_id, flags));
  return xnn_status_success;
}

xnn_status DefineXnnNodeForPool2d(xnn_subgraph_t subgraph,
                                  const MLOperator* pool2d,
                                  const OperandValueIdMap& operand_value_id_map,
                                  String& error_message) {
  const uint32_t input_id =
      GetOperatorInputValueId(pool2d, operand_value_id_map);
  const uint32_t output_id =
      GetOperatorOutputValueId(pool2d, operand_value_id_map);

  // Set strides of XNNPACK pooling 2d Node, default to 1.
  const MLPool2dOptions* options =
      static_cast<const MLPool2dOptions*>(pool2d->Options());
  const Vector<uint32_t> default_strides({1, 1});
  const uint32_t stride_height = options->getStridesOr(default_strides)[0];
  const uint32_t stride_width = options->getStridesOr(default_strides)[1];

  // Set dilations of XNNPACK pooling 2d Node, default to 1.
  const Vector<uint32_t> default_dilations({1, 1});
  const uint32_t dilation_height =
      options->getDilationsOr(default_dilations)[0];
  const uint32_t dilation_width = options->getDilationsOr(default_dilations)[1];

  // Set window sizes of XNNPACK pooling 2d Node.
  uint32_t input_height, input_width;
  uint32_t filter_height, filter_width;
  bool global_pooling = false;
  switch (options->layout().AsEnum()) {
    case V8MLInputOperandLayout::Enum::kNhwc: {
      const auto* input = pool2d->Inputs()[0].Get();
      DCHECK(input);
      input_height = input->Dimensions()[1];
      input_width = input->Dimensions()[2];
      if (options->hasWindowDimensions()) {
        filter_height = options->windowDimensions()[0];
        filter_width = options->windowDimensions()[1];
      } else {
        // According to WebNN pool2d spec:
        // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-pool2d, if the window
        // dimensions are not present, the window dimensions are assumed to be
        // the height and width dimensions of the input shape that could be
        // mapped to the global pooling operation.
        filter_height = input_height;
        filter_width = input_width;
        global_pooling = true;
      }
      break;
    }
    case V8MLInputOperandLayout::Enum::kNchw: {
      // TODO(crbug.com/1273291): support nchw input layout by transposing the
      // input tensor.
      error_message = "The nchw input layout is not supported.";
      return xnn_status_unsupported_parameter;
    }
  }

  // Set or calculate padding sizes of XNNPACK pooling 2d Node.
  const auto padding = GetXnnPadding2D(
      options, input_height, input_width, filter_height, filter_width,
      stride_height, stride_width, dilation_height, dilation_width);

  // Define XNNPACK average or max pooling 2d Node for the Subgraph object.
  const float output_min = -std::numeric_limits<float>::infinity();
  const float output_max = +std::numeric_limits<float>::infinity();
  const uint32_t flags = 0;
  switch (pool2d->Kind()) {
    case MLOperator::OperatorKind::kAveragePool2d: {
      if (dilation_height != 1 || dilation_width != 1) {
        error_message = "averagePool2d doesn't support dilations.";
        return xnn_status_unsupported_parameter;
      }
      if (global_pooling) {
        XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(
            xnn_define_global_average_pooling_2d(
                subgraph, output_min, output_max, input_id, output_id, flags));
      } else {
        XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(xnn_define_average_pooling_2d(
            subgraph, padding.top, padding.right, padding.bottom, padding.left,
            filter_height, filter_width, stride_height, stride_width,
            output_min, output_max, input_id, output_id, flags));
      }
      break;
    }
    case MLOperator::OperatorKind::kMaxPool2d: {
      XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(xnn_define_max_pooling_2d(
          subgraph, padding.top, padding.right, padding.bottom, padding.left,
          filter_height, filter_width, stride_height, stride_width,
          dilation_height, dilation_width, output_min, output_max, input_id,
          output_id, flags));
      break;
    }
    default:
      // Only average and max pool2d are supported by this method.
      NOTREACHED();
  }
  return xnn_status_success;
}

xnn_status DefineXnnNodeForRelu(xnn_subgraph_t subgraph,
                                const MLOperator* relu,
                                const OperandValueIdMap& operand_value_id_map,
                                String& error_message) {
  const uint32_t input_id = GetOperatorInputValueId(relu, operand_value_id_map);
  const uint32_t output_id =
      GetOperatorOutputValueId(relu, operand_value_id_map);
  const auto output_range = GetXnnOutputRangeForActivation(relu);
  const uint32_t flags = 0;
  XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(
      xnn_define_clamp(subgraph, output_range.min, output_range.max, input_id,
                       output_id, flags));
  return xnn_status_success;
}

xnn_status DefineXnnNodeForReshape(
    xnn_subgraph_t subgraph,
    const MLOperator* reshape,
    const OperandValueIdMap& operand_value_id_map,
    String& error_message) {
  const uint32_t input_id =
      GetOperatorInputValueId(reshape, operand_value_id_map);
  const uint32_t output_id =
      GetOperatorOutputValueId(reshape, operand_value_id_map);
  // Set the new shape of XNNPACK reshape Node to the output shape that is
  // already calculated by `MLGraphBuilder::reshape()`.
  Vector<size_t> new_shape;
  for (auto& d : reshape->Outputs()[0]->Dimensions()) {
    new_shape.push_back(base::checked_cast<size_t>(d));
  }
  const uint32_t flags = 0;
  // XNNPACK will memcpy the content of `new_shape` vector to its internal
  // structure, so it is safe to release `new_shape` vector after this call.
  // Please refer to the implementation at:
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/xnnpack/src/src/subgraph/static-reshape.c;l=246
  XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(
      xnn_define_static_reshape(subgraph, new_shape.size(), new_shape.data(),
                                input_id, output_id, flags));
  return xnn_status_success;
}

xnn_status DefineXnnNodeForSigmoid(
    xnn_subgraph_t subgraph,
    const MLOperator* sigmoid,
    const OperandValueIdMap& operand_value_id_map,
    String& error_message) {
  const uint32_t input_id =
      GetOperatorInputValueId(sigmoid, operand_value_id_map);
  const uint32_t output_id =
      GetOperatorOutputValueId(sigmoid, operand_value_id_map);
  const uint32_t flags = 0;
  XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(
      xnn_define_sigmoid(subgraph, input_id, output_id, flags));
  return xnn_status_success;
}

xnn_status DefineXnnNodeForSoftmax(
    xnn_subgraph_t subgraph,
    const MLOperator* softmax,
    const OperandValueIdMap& operand_value_id_map,
    String& error_message) {
  const uint32_t input_id =
      GetOperatorInputValueId(softmax, operand_value_id_map);
  const uint32_t output_id =
      GetOperatorOutputValueId(softmax, operand_value_id_map);
  const uint32_t flags = 0;
  XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(
      xnn_define_softmax(subgraph, input_id, output_id, flags));
  return xnn_status_success;
}

xnn_status DefineXnnNodeForResample2d(
    xnn_subgraph_t subgraph,
    const MLOperator* resample2d,
    const OperandValueIdMap& operand_value_id_map,
    String& error_message) {
  const uint32_t input_id =
      GetOperatorInputValueId(resample2d, operand_value_id_map);
  const uint32_t output_id =
      GetOperatorOutputValueId(resample2d, operand_value_id_map);
  const MLResample2dOptions* options =
      static_cast<const MLResample2dOptions*>(resample2d->Options());

  if (options->mode() != V8MLInterpolationMode::Enum::kLinear) {
    error_message = "Resample2d only supports Linear mode.";
    return xnn_status_unsupported_parameter;
  }

  const Vector<uint32_t> default_axes({2, 3});
  // XNNPACK resize bilinear node only supports axes = {1, 2}.
  // TODO(crbug.com/1273291): Support axes = {2, 3} by transposing the
  // input tensor.
  if (!(options->getAxesOr(default_axes)[0] == 1 &&
        options->getAxesOr(default_axes)[1] == 2)) {
    error_message = "Resample2d only supports axes = {1, 2}.";
    return xnn_status_unsupported_parameter;
  }

  DCHECK_EQ(resample2d->Outputs()[0]->Dimensions().size(), 4U);
  size_t output_height = resample2d->Outputs()[0]->Dimensions()[1];
  size_t output_width = resample2d->Outputs()[0]->Dimensions()[2];
  // Set flags = 0 and it means align_corner = false and half_pixel_center =
  // true. For WebNN, we plan to support coordinate transformation modes for
  // Resample2d and it's tracked by an issue -
  // https://github.com/webmachinelearning/webnn/issues/270.
  const uint32_t flags = 0;
  XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(xnn_define_static_resize_bilinear_2d(
      subgraph, output_height, output_width, input_id, output_id, flags));
  return xnn_status_success;
}

xnn_status DefineXnnNodeForTranspose(
    xnn_subgraph_t subgraph,
    const MLOperator* transpose,
    const OperandValueIdMap& operand_value_id_map,
    String& error_message) {
  const uint32_t input_id =
      GetOperatorInputValueId(transpose, operand_value_id_map);
  const uint32_t output_id =
      GetOperatorOutputValueId(transpose, operand_value_id_map);
  const MLTransposeOptions* options =
      static_cast<const MLTransposeOptions*>(transpose->Options());

  const auto* input = transpose->Inputs()[0].Get();
  CHECK(input);
  const auto input_rank = input->Dimensions().size();
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-transpose,
  // When permutation is not specified, it’s set to [N-1, ..., 0], where N is
  // the rank of the input tensor.
  Vector<uint32_t> default_permutation(input_rank);
  for (wtf_size_t i = 0; i < input_rank - 1; i++) {
    default_permutation[i] = input_rank - 1 - i;
  }
  const Vector<uint32_t> permutation =
      options->getPermutationOr(std::move(default_permutation));

  // The current WebNN spec defines the value of permutation as signed
  // integer: https://www.w3.org/TR/webnn/#dom-mltransposeoptions-permutation
  // And an issue has been filed to track it:
  // https://github.com/webmachinelearning/webnn/issues/317
  Vector<size_t> xnn_permutation(input_rank);
  base::ranges::transform(permutation, xnn_permutation.begin(), [](uint32_t p) {
    return base::checked_cast<size_t>(p);
  });
  const uint32_t flags = 0;
  // XNNPACK will memcpy the content of `xnn_permutation` vector to its internal
  // structure, so it is safe to release `xnn_permutation` vector after this
  // call. Please refer to the implementation at:
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/xnnpack/src/src/subgraph/static-transpose.c;l=267
  XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(xnn_define_static_transpose(
      subgraph, xnn_permutation.size(), xnn_permutation.data(), input_id,
      output_id, flags));
  return xnn_status_success;
}

// Helper to find the concat axis by comparing the first input shape and output
// shape which is already calculated by `MLGraphBuilder::concat()`.
absl::optional<uint32_t> GetConcatAxis(const MLOperator* concat) {
  // The output tensor should has the same shape size with all the input tensors
  const auto& output_dims = concat->Outputs()[0]->Dimensions();
  for (const auto& input : concat->Inputs()) {
    CHECK_EQ(input->Dimensions().size(), output_dims.size());
  }
  const auto& input_dims = concat->Inputs()[0]->Dimensions();
  for (wtf_size_t i = 0; i < input_dims.size(); ++i) {
    if (input_dims[i] != output_dims[i]) {
      return i;
    }
  }
  return absl::nullopt;
}

xnn_status DefineXnnNodeForConcat(xnn_subgraph_t subgraph,
                                  const MLOperator* concat,
                                  const OperandValueIdMap& operand_value_id_map,
                                  String& error_message) {
  const auto inputs_size = concat->Inputs().size();
  Vector<uint32_t> input_ids(inputs_size);
  for (uint32_t i = 0; i < inputs_size; ++i) {
    input_ids[i] = GetOperatorInputValueId(concat, operand_value_id_map, i);
  }
  const uint32_t output_id =
      GetOperatorOutputValueId(concat, operand_value_id_map);
  const uint32_t flags = 0;
  if (inputs_size == 1u) {
    // Use XNNPACK copy operator to supoprt single input.
    XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(
        xnn_define_copy(subgraph, input_ids[0], output_id, flags));
    return xnn_status_success;
  }
  absl::optional<uint32_t> axis = GetConcatAxis(concat);
  if (!axis) {
    error_message = "Can not find the concat axis.";
    return xnn_status_unsupported_parameter;
  }
  switch (inputs_size) {
    case 2u:
      XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(
          xnn_define_concatenate2(subgraph, axis.value(), input_ids[0],
                                  input_ids[1], output_id, flags));
      break;
    case 3u:
      XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(xnn_define_concatenate3(
          subgraph, axis.value(), input_ids[0], input_ids[1], input_ids[2],
          output_id, flags));
      break;
    case 4u:
      XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(xnn_define_concatenate4(
          subgraph, axis.value(), input_ids[0], input_ids[1], input_ids[2],
          input_ids[3], output_id, flags));
      break;
    default:
      // TODO(crbug.com/1273291): Consider decomposing the concat with inputs
      // size > 4 into multiple XNNPACK Concat Nodes.
      error_message = "XNNPACK backend doesn't support concat inputs size " +
                      String::Number(inputs_size);
      return xnn_status_unsupported_parameter;
  }
  return xnn_status_success;
}

// Define an XNNPACK Node given an MLOperator object and add it into the
// Subgraph object. The operand_value_id_map is used to find the corresponding
// input and output XNNPACK Values of this MLOperator object. This method calls
// the dedicated DefineXnnNode{OperatorName} helper method according to the kind
// of the MLOperator object.
xnn_status DefineXnnNode(xnn_subgraph_t subgraph,
                         const MLOperator* ml_operator,
                         const OperandValueIdMap& operand_value_id_map,
                         String& error_message) {
  switch (ml_operator->Kind()) {
    case MLOperator::OperatorKind::kClamp:
      XNN_CHECK_STATUS(DefineXnnNodeForClamp(
          subgraph, ml_operator, operand_value_id_map, error_message));
      break;
    case MLOperator::OperatorKind::kConv2d:
      XNN_CHECK_STATUS(DefineXnnNodeForConv2d(
          subgraph, ml_operator, operand_value_id_map, error_message));
      break;
    // Define XNNPACK Node for element-wise binary operators.
    case MLOperator::OperatorKind::kAdd:
    case MLOperator::OperatorKind::kSub:
    case MLOperator::OperatorKind::kMul:
    case MLOperator::OperatorKind::kDiv:
    case MLOperator::OperatorKind::kMax:
    case MLOperator::OperatorKind::kMin: {
      XNN_CHECK_STATUS(DefineXnnNodeForElementWiseBinary(
          subgraph, ml_operator, operand_value_id_map, error_message));
      break;
    }
    case MLOperator::OperatorKind::kGemm:
      XNN_CHECK_STATUS(DefineXnnNodeForGemm(
          subgraph, ml_operator, operand_value_id_map, error_message));
      break;
    case MLOperator::OperatorKind::kHardSwish:
      XNN_CHECK_STATUS(DefineXnnNodeForHardSwish(
          subgraph, ml_operator, operand_value_id_map, error_message));
      break;
    case MLOperator::OperatorKind::kPad:
      XNN_CHECK_STATUS(DefineXnnNodeForPad(
          subgraph, ml_operator, operand_value_id_map, error_message));
      break;
    // Define XNNPACK Node for pool2d operators.
    case MLOperator::OperatorKind::kAveragePool2d:
    case MLOperator::OperatorKind::kMaxPool2d: {
      XNN_CHECK_STATUS(DefineXnnNodeForPool2d(
          subgraph, ml_operator, operand_value_id_map, error_message));
      break;
    }
    case MLOperator::OperatorKind::kLeakyRelu:
      XNN_CHECK_STATUS(DefineXnnNodeForLeakyRelu(
          subgraph, ml_operator, operand_value_id_map, error_message));
      break;
    case MLOperator::OperatorKind::kRelu:
      XNN_CHECK_STATUS(DefineXnnNodeForRelu(
          subgraph, ml_operator, operand_value_id_map, error_message));
      break;
    case MLOperator::OperatorKind::kReshape:
      XNN_CHECK_STATUS(DefineXnnNodeForReshape(
          subgraph, ml_operator, operand_value_id_map, error_message));
      break;
    case MLOperator::OperatorKind::kSigmoid:
      XNN_CHECK_STATUS(DefineXnnNodeForSigmoid(
          subgraph, ml_operator, operand_value_id_map, error_message));
      break;
    case MLOperator::OperatorKind::kSoftmax:
      XNN_CHECK_STATUS(DefineXnnNodeForSoftmax(
          subgraph, ml_operator, operand_value_id_map, error_message));
      break;
    case MLOperator::OperatorKind::kResample2d: {
      XNN_CHECK_STATUS(DefineXnnNodeForResample2d(
          subgraph, ml_operator, operand_value_id_map, error_message));
      break;
    }
    case MLOperator::OperatorKind::kTranspose: {
      XNN_CHECK_STATUS(DefineXnnNodeForTranspose(
          subgraph, ml_operator, operand_value_id_map, error_message));
      break;
    }
    case MLOperator::OperatorKind::kConcat: {
      XNN_CHECK_STATUS(DefineXnnNodeForConcat(
          subgraph, ml_operator, operand_value_id_map, error_message));
      break;
    }
    default: {
      error_message = "The operator (" +
                      MLOperator::OperatorKindToString(ml_operator->Kind()) +
                      ") is not supported.";
      return xnn_status_unsupported_parameter;
    }
  }
  return xnn_status_success;
}

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

MLGraphXnnpack::MLGraphXnnpack(MLContext* context)
    : MLGraph(context),
      xnnpack_task_runner_(worker_pool::CreateSequencedTaskRunner({})) {
  auto* execution_context = context->GetML()->GetExecutionContext();
  CHECK(execution_context);
  // TODO(crbug.com/1273291): Get a dedicated queue when the specification
  // matures.
  resolver_task_runner_ =
      execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI);
}

MLGraphXnnpack::~MLGraphXnnpack() = default;

const ExternalValueIdMap& MLGraphXnnpack::GetInputExternalValueIdMapForTesting()
    const {
  return input_external_value_id_map_;
}

const ExternalValueIdMap&
MLGraphXnnpack::GetOutputExternalValueIdMapForTesting() const {
  return output_external_value_id_map_;
}

const Vector<xnn_external_value>& MLGraphXnnpack::GetXnnExternalValuesTesting()
    const {
  return xnn_runtime_wrapper_->GetXnnExternalValuesTesting();
}

void MLGraphXnnpack::BuildAsyncImpl(const MLNamedOperands& named_outputs,
                                    ScriptPromiseResolver* resolver) {
  CHECK(IsMainThread());
  CHECK(!xnn_runtime_wrapper_);
  PostCrossThreadTask(
      *xnnpack_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &GetSharedXnnpackContextOnBackgroundThread,
          MakeCrossThreadHandle(this),
          MakeCrossThreadHandle(
              MakeGarbageCollected<MLNamedOperands>(named_outputs)),
          MakeCrossThreadHandle(resolver), resolver_task_runner_));
}

// static
void MLGraphXnnpack::GetSharedXnnpackContextOnBackgroundThread(
    CrossThreadHandle<MLGraphXnnpack> graph,
    CrossThreadHandle<MLNamedOperands> named_outputs,
    CrossThreadHandle<ScriptPromiseResolver> resolver,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner) {
  CHECK(!IsMainThread());
  // Get or create the SharedXnnpackContext.
  String error_message;
  auto xnn_context = SharedXnnpackContext::GetInstance(error_message);
  PostCrossThreadTask(
      *resolver_task_runner, FROM_HERE,
      CrossThreadBindOnce(
          &MLGraphXnnpack::OnDidGetSharedXnnpackContext,
          MakeUnwrappingCrossThreadHandle(std::move(graph)),
          std::move(xnn_context),
          MakeUnwrappingCrossThreadHandle(std::move(named_outputs)),
          MakeUnwrappingCrossThreadHandle(std::move(resolver)),
          std::move(error_message)));
}

void MLGraphXnnpack::OnDidGetSharedXnnpackContext(
    scoped_refptr<SharedXnnpackContext> xnn_context,
    MLNamedOperands* named_outputs,
    ScriptPromiseResolver* resolver,
    String error_message) {
  CHECK(IsMainThread());
  if (!xnn_context) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        XnnStatusToDOMExceptionCode(xnn_status_uninitialized), error_message));
    return;
  }

  Vector<DataBufferPtr> static_data_buffers;
  XnnSubgraphPtr subgraph(nullptr, &xnn_delete_subgraph);
  xnn_status status = CreateXnnSubgraph(*named_outputs, subgraph,
                                        static_data_buffers, error_message);
  if (status != xnn_status_success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        XnnStatusToDOMExceptionCode(status), error_message));
    return;
  }
  // Pass `xnn_context` and `static_data_buffers` forward for XNNPACK Runtime
  // creation. If it succeeds, they will be retained by an `XnnRuntimeWrapper`
  // object within this `MLGraphXnnpack`.
  PostCrossThreadTask(
      *xnnpack_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &CreateXnnRuntimeOnBackgroundThread, std::move(subgraph),
          std::move(xnn_context), std::move(static_data_buffers),
          MakeCrossThreadHandle(this), MakeCrossThreadHandle(resolver),
          resolver_task_runner_));
}

// static
void MLGraphXnnpack::CreateXnnRuntimeOnBackgroundThread(
    XnnSubgraphPtr subgraph,
    scoped_refptr<SharedXnnpackContext> xnn_context,
    Vector<DataBufferPtr> static_data_buffers,
    CrossThreadHandle<MLGraphXnnpack> graph,
    CrossThreadHandle<ScriptPromiseResolver> resolver,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner) {
  CHECK(!IsMainThread());
  String error_message;
  auto xnn_runtime_wrapper =
      XnnRuntimeWrapper::Create(std::move(subgraph), std::move(xnn_context),
                                std::move(static_data_buffers), error_message);
  PostCrossThreadTask(
      *resolver_task_runner, FROM_HERE,
      CrossThreadBindOnce(&MLGraphXnnpack::OnDidCreateXnnRuntime,
                          MakeUnwrappingCrossThreadHandle(std::move(graph)),
                          std::move(xnn_runtime_wrapper),
                          MakeUnwrappingCrossThreadHandle(std::move(resolver)),
                          std::move(error_message)));
}

void MLGraphXnnpack::OnDidCreateXnnRuntime(
    scoped_refptr<XnnRuntimeWrapper> xnn_runtime_wrapper,
    ScriptPromiseResolver* resolver,
    String error_message) {
  CHECK(IsMainThread());
  if (!xnn_runtime_wrapper) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError, error_message));
    return;
  }
  xnn_runtime_wrapper_ = std::move(xnn_runtime_wrapper);
  resolver->Resolve(this);
}

MLGraph* MLGraphXnnpack::BuildSyncImpl(const MLNamedOperands& named_outputs,
                                       ExceptionState& exception_state) {
  CHECK(!xnn_runtime_wrapper_);
  String error_message;
  auto xnn_context = SharedXnnpackContext::GetInstance(error_message);
  if (!xnn_context) {
    exception_state.ThrowDOMException(
        XnnStatusToDOMExceptionCode(xnn_status_uninitialized), error_message);
    return nullptr;
  }
  Vector<DataBufferPtr> static_data_buffers;
  XnnSubgraphPtr subgraph(nullptr, &xnn_delete_subgraph);
  xnn_status status = CreateXnnSubgraph(named_outputs, subgraph,
                                        static_data_buffers, error_message);
  if (status != xnn_status_success) {
    exception_state.ThrowDOMException(XnnStatusToDOMExceptionCode(status),
                                      error_message);
    return nullptr;
  }
  xnn_runtime_wrapper_ =
      XnnRuntimeWrapper::Create(std::move(subgraph), std::move(xnn_context),
                                std::move(static_data_buffers), error_message);
  if (!xnn_runtime_wrapper_) {
    exception_state.ThrowDOMException(
        XnnStatusToDOMExceptionCode(xnn_status_invalid_parameter),
        error_message);
    return nullptr;
  }

  return this;
}

void MLGraphXnnpack::ComputeAsyncImpl(const MLNamedArrayBufferViews& inputs,
                                      const MLNamedArrayBufferViews& outputs,
                                      ScriptPromiseResolver* resolver,
                                      ExceptionState& exception_state) {
  // `MLNamedArrayBufferViews` objects should be accessed on the thread owning
  // the heap before transferring.
  auto external_values = CreateExternalValues(inputs, outputs);

  // Transfer the `MLNamedArrayBufferViews` to `NamedArrayBufferViewsInfo` which
  // is safe to be posted to a worker thread.
  auto inputs_info = TransferNamedArrayBufferViews(
      resolver->GetScriptState()->GetIsolate(), inputs, exception_state);
  if (!inputs_info) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError,
        "Invalid inputs: " + exception_state.Message()));
    return;
  }
  auto outputs_info = TransferNamedArrayBufferViews(
      resolver->GetScriptState()->GetIsolate(), outputs, exception_state);
  if (!outputs_info) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError,
        "Invalid outputs: " + exception_state.Message()));
    return;
  }

  // Pass `inputs_info` and `outputs_info` forward for `MLNamedArrayBufferViews`
  // re-creation in `OnDidCompute()`.
  PostCrossThreadTask(
      *xnnpack_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&ComputeOnBackgroundThread, xnn_runtime_wrapper_,
                          std::move(external_values), std::move(inputs_info),
                          std::move(outputs_info), MakeCrossThreadHandle(this),
                          MakeCrossThreadHandle(resolver),
                          resolver_task_runner_));
}

// static
void MLGraphXnnpack::ComputeOnBackgroundThread(
    scoped_refptr<XnnRuntimeWrapper> xnn_runtime_wrapper,
    XnnExternalValuesPtr external_values,
    NamedArrayBufferViewsInfoPtr inputs_info,
    NamedArrayBufferViewsInfoPtr outputs_info,
    CrossThreadHandle<MLGraphXnnpack> graph,
    CrossThreadHandle<ScriptPromiseResolver> resolver,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner) {
  CHECK(!IsMainThread());
  String error_message;
  xnn_status status =
      xnn_runtime_wrapper->Invoke(std::move(external_values), error_message);
  PostCrossThreadTask(
      *resolver_task_runner, FROM_HERE,
      CrossThreadBindOnce(&MLGraphXnnpack::OnDidCompute,
                          MakeUnwrappingCrossThreadHandle(std::move(graph)),
                          status, std::move(inputs_info),
                          std::move(outputs_info),
                          MakeUnwrappingCrossThreadHandle(std::move(resolver)),
                          std::move(error_message)));
}

void MLGraphXnnpack::OnDidCompute(xnn_status status,
                                  NamedArrayBufferViewsInfoPtr inputs_info,
                                  NamedArrayBufferViewsInfoPtr outputs_info,
                                  ScriptPromiseResolver* resolver,
                                  String error_message) {
  if (status != xnn_status_success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        XnnStatusToDOMExceptionCode(status), error_message));
    return;
  }

  auto* result = MLComputeResult::Create();
  // Create MLNamedArrayBufferViews from NamedArrayBufferViewsInfo.
  result->setInputs(*CreateNamedArrayBufferViews(std::move(inputs_info)));
  result->setOutputs(*CreateNamedArrayBufferViews(std::move(outputs_info)));
  resolver->Resolve(result);
}

void MLGraphXnnpack::ComputeSyncImpl(const MLNamedArrayBufferViews& inputs,
                                     const MLNamedArrayBufferViews& outputs,
                                     ExceptionState& exception_state) {
  auto external_values = CreateExternalValues(inputs, outputs);
  String error_message;
  xnn_status status =
      xnn_runtime_wrapper_->Invoke(std::move(external_values), error_message);
  if (status != xnn_status_success) {
    exception_state.ThrowDOMException(XnnStatusToDOMExceptionCode(status),
                                      error_message);
  }
}

xnn_status MLGraphXnnpack::CreateXnnSubgraph(
    const MLNamedOperands& named_outputs,
    XnnSubgraphPtr& out_subgraph,
    Vector<DataBufferPtr>& out_static_data_buffers,
    String& error_message) {
  TRACE_EVENT("blink", "MLGraphXnnpack::CreateXnnSubgraph");

  // The number of external value IDs that is reserved by XNNPACK Subgraph. Set
  // its value to the number of graph input and output resources.
  uint32_t external_value_ids_num;
  if (!base::CheckAdd<wtf_size_t>(input_resources_info_.size(),
                                  output_resources_info_.size())
           .AssignIfValid(&external_value_ids_num)) {
    error_message = "The graph has too many inputs and outputs.";
    return xnn_status_invalid_parameter;
  }
  xnn_subgraph_t subgraph_ptr = nullptr;
  XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(
      xnn_create_subgraph(external_value_ids_num, 0, &subgraph_ptr));
  CHECK(subgraph_ptr);
  XnnSubgraphPtr subgraph(subgraph_ptr, &xnn_delete_subgraph);

  // Holds the static data of XNNPACK Values for MLGraph's constant operands.
  Vector<DataBufferPtr> static_data_buffers;
  // Map the operand to its XNNPACK Value ID.
  OperandValueIdMap operand_value_id_map;
  // The ID is used to define an external XNNPACK Value. It should be increased
  // by 1 after each definition.
  uint32_t external_value_id = 0;

  for (const auto& output : named_outputs) {
    // Define an external XNNPACK Value for the graph's output operand.
    const auto& [name, operand] = output;
    // The external Value ID should be in the [0, external_value_ids_num - 1]
    // range.
    DCHECK_LT(external_value_id, external_value_ids_num);
    uint32_t value_id;
    XNN_CHECK_STATUS(DefineExternalXnnValue(
        subgraph.get(), operand, external_value_id, value_id, error_message));
    // If the external Value ID is provided, the value_id should be set to that
    // ID.
    DCHECK_EQ(external_value_id, value_id);
    // Increase the ID by 1 for defining the next external Value.
    external_value_id++;
    operand_value_id_map.insert(operand.Get(), value_id);
    output_external_value_id_map_.insert(name, value_id);
  }

  // TODO(crbug.com/1273291): Revisit whether the topological sorting should run
  // in the worker thread.
  auto* toposorted_operators = GetOperatorsInTopologicalOrder(named_outputs);
  CHECK(toposorted_operators);

  // Visit the operators in topological order. For each operator, define XNNPACK
  // Values for its input and output operands.
  for (const auto current_operator : *toposorted_operators) {
    for (const auto& operand : current_operator->Inputs()) {
      if (operand_value_id_map.Contains(operand.Get())) {
        // The XNNPACK Value is already defined for this operand, skip it.
        continue;
      }
      switch (operand->Kind()) {
        case MLOperand::OperandKind::kInput: {
          // Define an external XNNPACK Value for the graph's input operand.
          // The external ID should be in the [0, external_value_ids_num - 1]
          // range.
          DCHECK_LT(external_value_id, external_value_ids_num);
          uint32_t value_id;
          XNN_CHECK_STATUS(DefineExternalXnnValue(subgraph.get(), operand,
                                                  external_value_id, value_id,
                                                  error_message));
          // If the external Value ID is provided, the value_id should be set to
          // that ID.
          DCHECK_EQ(external_value_id, value_id);
          // Increase the ID by 1 for defining the next external Value.
          external_value_id++;
          operand_value_id_map.insert(operand.Get(), value_id);
          input_external_value_id_map_.insert(operand->Name(), value_id);
          break;
        }
        case MLOperand::OperandKind::kConstant: {
          // Define a static XNNPACK Value for this constant operand. Because
          // XNNPACK requires the static data of a static XNNPACK Value must
          // exceed the life-time of its Subgraph and Runtime objects, a new
          // buffer is allocated and kept alive by this MLGraphXnnpack object.
          // The contents of this constant operand are copied from the array
          // buffer into the newly-allocated buffer and it is used to initialize
          // the XNNPACK Value.
          const auto* array_buffer_view = operand->ArrayBufferView();
          auto data =
              std::make_unique<uint8_t[]>(array_buffer_view->byteLength());
          DCHECK(data);
          memcpy(data.get(), array_buffer_view->BaseAddress(),
                 array_buffer_view->byteLength());
          uint32_t value_id;
          XNN_CHECK_STATUS(DefineStaticXnnValue(subgraph.get(), operand, data,
                                                value_id, error_message));
          operand_value_id_map.insert(operand.Get(), value_id);
          static_data_buffers.push_back(std::move(data));
          break;
        }
        case MLOperand::OperandKind::kOutput:
          // Because the operators are visited in topological order, if this
          // operand is an intermediate operand, it should already be defined as
          // an output operand of the dependent operator.
          NOTREACHED();
          break;
      }
    }

    for (const auto& operand : current_operator->Outputs()) {
      if (operand_value_id_map.Contains(operand.Get())) {
        // If the XNNPACK Value is already defined for this operand, skip it.
        continue;
      }
      // Because the graph's output operands are already defined before, this
      // operand should be an intermediate operand that connects with two
      // operators. Define an internal XNNPACK Value for this operand.
      uint32_t value_id;
      XNN_CHECK_STATUS(DefineInternalXnnValue(subgraph.get(), operand, value_id,
                                              error_message));
      operand_value_id_map.insert(operand.Get(), value_id);
    }

    XNN_CHECK_STATUS(DefineXnnNode(subgraph.get(), current_operator,
                                   operand_value_id_map, error_message));
  }

  // Return the XNNPACK Subgraph and static data buffers if there are no errors.
  out_subgraph = std::move(subgraph);
  out_static_data_buffers = std::move(static_data_buffers);
  return xnn_status_success;
}

XnnExternalValuesPtr MLGraphXnnpack::CreateExternalValues(
    const MLNamedArrayBufferViews& inputs,
    const MLNamedArrayBufferViews& outputs) const {
  auto external_values = std::make_unique<Vector<xnn_external_value>>();
  external_values->reserve((inputs.size() + outputs.size()));
  // Although XNNPACK doesn't validate the pointers, the base address and the
  // byte length of the array buffer views are already validated by
  // ValidateNamedArrayBufferViews(). It should be safe to setup XNNPACK Runtime
  // object with them.
  for (const auto& [name, array_buffer_view] : inputs) {
    DCHECK(input_external_value_id_map_.Contains(name));
    external_values->emplace_back(
        xnn_external_value{.id = input_external_value_id_map_.at(name),
                           .data = array_buffer_view->BaseAddress()});
  }
  for (const auto& [name, array_buffer_view] : outputs) {
    DCHECK(output_external_value_id_map_.Contains(name));
    external_values->emplace_back(
        xnn_external_value{.id = output_external_value_id_map_.at(name),
                           .data = array_buffer_view->BaseAddress()});
  }
  base::ranges::sort(*external_values, base::ranges::less{},
                     &xnn_external_value::id);
  return external_values;
}

}  // namespace blink
