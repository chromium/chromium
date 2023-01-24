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
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
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

// Maps MLOperand pointer address to its XNNPACK Value ID.
//
// Use `const void*` here because this HashMap might be used in a worker thread
// that doesn't support GC.
//
// This map is only used in CreateXnnSubgraphAndRuntime(), who owns references
// to MLOperands, so it's safe to use raw pointers here.
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
      auto padding_sizes_height = MLGraphBuilder::CalculatePaddingForAutoPad(
          options->autoPad().AsEnum(), input_height, filter_height,
          stride_height, dilation_height);
      DCHECK(padding_sizes_height);
      xnn_padding.top = padding_sizes_height.value().begin;
      xnn_padding.bottom = padding_sizes_height.value().end;
      auto padding_sizes_width = MLGraphBuilder::CalculatePaddingForAutoPad(
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
    switch (options->activation()->Kind()) {
      case MLOperator::OperatorKind::kClamp:
      case MLOperator::OperatorKind::kRelu:
        output_range = GetXnnOutputRangeForActivation(options->activation());
        break;
      default:
        error_message =
            "The fused operator (" +
            MLOperator::OperatorKindToString(options->activation()->Kind()) +
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
    // Define XNNPACK Node for pool2d operators.
    case MLOperator::OperatorKind::kAveragePool2d:
    case MLOperator::OperatorKind::kMaxPool2d: {
      XNN_CHECK_STATUS(DefineXnnNodeForPool2d(
          subgraph, ml_operator, operand_value_id_map, error_message));
      break;
    }
    case MLOperator::OperatorKind::kRelu:
      XNN_CHECK_STATUS(DefineXnnNodeForRelu(
          subgraph, ml_operator, operand_value_id_map, error_message));
      break;
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

MLGraphXnnpack::MLGraphXnnpack(MLContext* context) : MLGraph(context) {}

MLGraphXnnpack::~MLGraphXnnpack() {
  // Explicitly destroy XNNPACK Runtime before releasing static data buffers. It
  // ensures the lifetime of static data buffers exceeds the lifetime of this
  // Runtime object.
  xnn_runtime_.reset();
  static_data_buffers_.clear();
}

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
  return xnn_external_values_;
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
          WrapCrossThreadPersistent(
              MakeGarbageCollected<MLNamedOperands>(named_outputs)),
          WrapCrossThreadPersistent(toposorted_operators),
          WrapCrossThreadPersistent(resolver), std::move(task_runner)));
}

// static
void MLGraphXnnpack::BuildOnBackgroundThread(
    CrossThreadPersistent<MLGraphXnnpack> graph,
    CrossThreadPersistent<MLNamedOperands> named_outputs,
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

  status = graph->CreateXnnSubgraphAndRuntime(
      *named_outputs, *toposorted_operators, error_message);

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

  auto* toposorted_operators = GetOperatorsInTopologicalOrder(named_outputs);
  xnn_status status = CreateXnnSubgraphAndRuntime(
      named_outputs, *toposorted_operators, error_message);
  if (status != xnn_status_success) {
    exception_state.ThrowDOMException(XnnStatusToDOMExceptionCode(status),
                                      error_message);
    return nullptr;
  }

  return this;
}

void MLGraphXnnpack::ComputeAsyncImpl(const MLNamedArrayBufferViews& inputs,
                                      const MLNamedArrayBufferViews& outputs,
                                      ScriptPromiseResolver* resolver) {
  // TODO(crbug.com/1273291): There is an issue of current WebNN asynchronous
  // execution design: https://github.com/webmachinelearning/webnn/issues/318.
  // After the spec issue is fixed, implement this method by posting the inputs
  // and outputs to a background thread and invoking XNNPACK Runtime object in
  // the background thread.

  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError, "Not implemented."));
}

void MLGraphXnnpack::ComputeSyncImpl(const MLNamedArrayBufferViews& inputs,
                                     const MLNamedArrayBufferViews& outputs,
                                     ExceptionState& exception_state) {
  String error_message;
  xnn_status status = InvokeXnnRuntime(inputs, outputs, error_message);
  if (status != xnn_status_success) {
    exception_state.ThrowDOMException(XnnStatusToDOMExceptionCode(status),
                                      error_message);
  }
}

xnn_status MLGraphXnnpack::CreateXnnSubgraphAndRuntime(
    const MLNamedOperands& named_outputs,
    const HeapVector<Member<const MLOperator>>& toposorted_operators,
    String& error_message) {
  TRACE_EVENT("blink", "MLGraphXnnpack::CreateXnnSubgraphAndRuntime");

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
  DCHECK_NE(subgraph_ptr, nullptr);

  // XNNPACK Subgraph is an abstract representation of a neural network model.
  // The Subgraph Values and Nodes will be defined for the operands and
  // operators of a WebNN graph. An XNNPACK Runtime object will be created from
  // the Subgraph object. Once constructed, the Runtime object is independent of
  // the Subgraph object. The Runtime object is kept for the accelerated
  // executions and the Subgraph object will be deleted.
  std::unique_ptr<xnn_subgraph, decltype(&xnn_delete_subgraph)> subgraph(
      subgraph_ptr, &xnn_delete_subgraph);

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

  // Visit the operators in topological order. For each operator, define XNNPACK
  // Values for its input and output operands.
  for (const auto current_operator : toposorted_operators) {
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
          static_data_buffers_.push_back(std::move(data));
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

  xnn_runtime_t runtime_ptr = nullptr;
  XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(
      xnn_create_runtime(subgraph.get(), &runtime_ptr));
  DCHECK_NE(runtime_ptr, nullptr);
  xnn_runtime_.reset(runtime_ptr);
  return xnn_status_success;
}

Vector<xnn_external_value> MLGraphXnnpack::CreateExternalValues(
    const MLNamedArrayBufferViews& inputs,
    const MLNamedArrayBufferViews& outputs) const {
  Vector<xnn_external_value> external_values;
  external_values.reserve((inputs.size() + outputs.size()));
  // Although XNNPACK doesn't validate the pointers, the base address and the
  // byte length of the array buffer views are already validated by
  // ValidateNamedArrayBufferViews(). It should be safe to setup XNNPACK Runtime
  // object with them.
  for (const auto& [name, array_buffer_view] : inputs) {
    DCHECK(input_external_value_id_map_.Contains(name));
    external_values.emplace_back(
        xnn_external_value{.id = input_external_value_id_map_.at(name),
                           .data = array_buffer_view->BaseAddress()});
  }
  for (const auto& [name, array_buffer_view] : outputs) {
    DCHECK(output_external_value_id_map_.Contains(name));
    external_values.emplace_back(
        xnn_external_value{.id = output_external_value_id_map_.at(name),
                           .data = array_buffer_view->BaseAddress()});
  }
  base::ranges::sort(external_values, base::ranges::less{},
                     &xnn_external_value::id);
  return external_values;
}

bool MLGraphXnnpack::NeedToSetupExternalValues(
    const Vector<xnn_external_value>& external_values) const {
  return !base::ranges::equal(external_values, xnn_external_values_,
                              [](const auto& a, const auto& b) {
                                return a.id == b.id && a.data == b.data;
                              });
}

xnn_status MLGraphXnnpack::InvokeXnnRuntime(
    const MLNamedArrayBufferViews& inputs,
    const MLNamedArrayBufferViews& outputs,
    String& error_message) {
  TRACE_EVENT("blink", "MLGraphXnnpack::InvokeXnnRuntime");

  auto external_values = CreateExternalValues(inputs, outputs);
  if (NeedToSetupExternalValues(external_values)) {
    XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(xnn_setup_runtime(
        xnn_runtime_.get(), external_values.size(), external_values.data()));
    xnn_external_values_ = external_values;
  }

  XNN_CHECK_STATUS_AND_SET_ERROR_MESSAGE(
      xnn_invoke_runtime(xnn_runtime_.get()));
  return xnn_status_success;
}

}  // namespace blink
