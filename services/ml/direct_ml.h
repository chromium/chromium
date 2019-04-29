//  Copyright (c) Microsoft Corporation.  All rights reserved.

#ifndef DIRECTML_H
#define DIRECTML_H
#pragma once

#include "d3d12.h"

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP | WINAPI_PARTITION_GAMES)

#ifndef DML_DECLARE_INTERFACE
#define DML_DECLARE_INTERFACE(iid) DECLSPEC_UUID(iid) DECLSPEC_NOVTABLE
#endif

// ===================================================================================================================
//   DirectML constants
// ===================================================================================================================

static const UINT DML_TENSOR_DIMENSION_COUNT_MAX = 5;

static const UINT DML_TEMPORARY_BUFFER_ALIGNMENT = 256;
static const UINT DML_PERSISTENT_BUFFER_ALIGNMENT = 256;

static const UINT DML_MINIMUM_BUFFER_TENSOR_ALIGNMENT = 16;


// ===================================================================================================================
//   Interface declarations
// ===================================================================================================================

interface IDMLObject;
interface IDMLDevice;
interface IDMLDeviceChild;
interface IDMLPageable;
interface IDMLDispatchable;
interface IDMLOperator;
interface IDMLCompiledOperator;
interface IDMLOperatorInitializer;
interface IDMLBindingTable;
interface IDMLCommandRecorder;


// ===================================================================================================================
//   Tensor descriptions
// ===================================================================================================================

enum DML_TENSOR_DATA_TYPE
{
    DML_TENSOR_DATA_TYPE_UNKNOWN,
    DML_TENSOR_DATA_TYPE_FLOAT32,
    DML_TENSOR_DATA_TYPE_FLOAT16,
    DML_TENSOR_DATA_TYPE_UINT32,
    DML_TENSOR_DATA_TYPE_UINT16,
    DML_TENSOR_DATA_TYPE_UINT8,
    DML_TENSOR_DATA_TYPE_INT32,
    DML_TENSOR_DATA_TYPE_INT16,
    DML_TENSOR_DATA_TYPE_INT8,
};

enum DML_TENSOR_TYPE
{
    DML_TENSOR_TYPE_INVALID,

    DML_TENSOR_TYPE_BUFFER,
};

enum DML_TENSOR_FLAGS
{
    DML_TENSOR_FLAG_NONE = 0x0,
    DML_TENSOR_FLAG_OWNED_BY_DML = 0x1,
};

DEFINE_ENUM_FLAG_OPERATORS(DML_TENSOR_FLAGS)

struct DML_BUFFER_TENSOR_DESC
{
    DML_TENSOR_DATA_TYPE DataType;
    DML_TENSOR_FLAGS Flags;
    UINT DimensionCount;
    _In_reads_(DimensionCount) const UINT* Sizes;
    _In_reads_opt_(DimensionCount) const UINT* Strides;
    UINT64 TotalTensorSizeInBytes;
    UINT GuaranteedBaseOffsetAlignment;
};

struct DML_TENSOR_DESC
{
    DML_TENSOR_TYPE Type;
    _In_reads_(_Inexpressible_("Dependent on tensor type")) const void* Desc;
};


// ===================================================================================================================
//   Operator types
// ===================================================================================================================

enum DML_OPERATOR_TYPE
{
    DML_OPERATOR_INVALID,

    DML_OPERATOR_ELEMENT_WISE_IDENTITY,
    DML_OPERATOR_ELEMENT_WISE_ABS,
    DML_OPERATOR_ELEMENT_WISE_ACOS,
    DML_OPERATOR_ELEMENT_WISE_ADD,
    DML_OPERATOR_ELEMENT_WISE_ASIN,
    DML_OPERATOR_ELEMENT_WISE_ATAN,
    DML_OPERATOR_ELEMENT_WISE_CEIL,
    DML_OPERATOR_ELEMENT_WISE_CLIP,
    DML_OPERATOR_ELEMENT_WISE_COS,
    DML_OPERATOR_ELEMENT_WISE_DIVIDE,
    DML_OPERATOR_ELEMENT_WISE_EXP,
    DML_OPERATOR_ELEMENT_WISE_FLOOR,
    DML_OPERATOR_ELEMENT_WISE_LOG,
    DML_OPERATOR_ELEMENT_WISE_LOGICAL_AND,
    DML_OPERATOR_ELEMENT_WISE_LOGICAL_EQUALS,
    DML_OPERATOR_ELEMENT_WISE_LOGICAL_GREATER_THAN,
    DML_OPERATOR_ELEMENT_WISE_LOGICAL_LESS_THAN,
    DML_OPERATOR_ELEMENT_WISE_LOGICAL_NOT,
    DML_OPERATOR_ELEMENT_WISE_LOGICAL_OR,
    DML_OPERATOR_ELEMENT_WISE_LOGICAL_XOR,
    DML_OPERATOR_ELEMENT_WISE_MAX,
    DML_OPERATOR_ELEMENT_WISE_MEAN,
    DML_OPERATOR_ELEMENT_WISE_MIN,
    DML_OPERATOR_ELEMENT_WISE_MULTIPLY,
    DML_OPERATOR_ELEMENT_WISE_POW,
    DML_OPERATOR_ELEMENT_WISE_CONSTANT_POW,
    DML_OPERATOR_ELEMENT_WISE_RECIP,
    DML_OPERATOR_ELEMENT_WISE_SIN,
    DML_OPERATOR_ELEMENT_WISE_SQRT,
    DML_OPERATOR_ELEMENT_WISE_SUBTRACT,
    DML_OPERATOR_ELEMENT_WISE_TAN,
    DML_OPERATOR_ELEMENT_WISE_THRESHOLD,
    DML_OPERATOR_ELEMENT_WISE_QUANTIZE_LINEAR,
    DML_OPERATOR_ELEMENT_WISE_DEQUANTIZE_LINEAR,
    DML_OPERATOR_ACTIVATION_ELU,
    DML_OPERATOR_ACTIVATION_HARDMAX,
    DML_OPERATOR_ACTIVATION_HARD_SIGMOID,
    DML_OPERATOR_ACTIVATION_IDENTITY,
    DML_OPERATOR_ACTIVATION_LEAKY_RELU,
    DML_OPERATOR_ACTIVATION_LINEAR,
    DML_OPERATOR_ACTIVATION_LOG_SOFTMAX,
    DML_OPERATOR_ACTIVATION_PARAMETERIZED_RELU,
    DML_OPERATOR_ACTIVATION_PARAMETRIC_SOFTPLUS,
    DML_OPERATOR_ACTIVATION_RELU,
    DML_OPERATOR_ACTIVATION_SCALED_ELU,
    DML_OPERATOR_ACTIVATION_SCALED_TANH,
    DML_OPERATOR_ACTIVATION_SIGMOID,
    DML_OPERATOR_ACTIVATION_SOFTMAX,
    DML_OPERATOR_ACTIVATION_SOFTPLUS,
    DML_OPERATOR_ACTIVATION_SOFTSIGN,
    DML_OPERATOR_ACTIVATION_TANH,
    DML_OPERATOR_ACTIVATION_THRESHOLDED_RELU,
    DML_OPERATOR_CONVOLUTION,
    DML_OPERATOR_GEMM,
    DML_OPERATOR_REDUCE,
    DML_OPERATOR_AVERAGE_POOLING,
    DML_OPERATOR_LP_POOLING,
    DML_OPERATOR_MAX_POOLING,
    DML_OPERATOR_ROI_POOLING,
    DML_OPERATOR_SLICE,
    DML_OPERATOR_CAST,
    DML_OPERATOR_SPLIT,
    DML_OPERATOR_JOIN,
    DML_OPERATOR_PADDING,
    DML_OPERATOR_VALUE_SCALE_2D,
    DML_OPERATOR_UPSAMPLE_2D,
    DML_OPERATOR_GATHER,
    DML_OPERATOR_SPACE_TO_DEPTH,
    DML_OPERATOR_DEPTH_TO_SPACE,
    DML_OPERATOR_TILE,
    DML_OPERATOR_TOP_K,
    DML_OPERATOR_BATCH_NORMALIZATION,
    DML_OPERATOR_MEAN_VARIANCE_NORMALIZATION,
    DML_OPERATOR_LOCAL_RESPONSE_NORMALIZATION,
    DML_OPERATOR_LP_NORMALIZATION,
    DML_OPERATOR_RNN,
    DML_OPERATOR_LSTM,
    DML_OPERATOR_GRU,
};


// ===================================================================================================================
//   Operator enumerations and structures
// ===================================================================================================================

enum DML_REDUCE_FUNCTION
{
    DML_REDUCE_FUNCTION_ARGMAX,
    DML_REDUCE_FUNCTION_ARGMIN,
    DML_REDUCE_FUNCTION_AVERAGE,
    DML_REDUCE_FUNCTION_L1,
    DML_REDUCE_FUNCTION_L2,
    DML_REDUCE_FUNCTION_LOG_SUM,
    DML_REDUCE_FUNCTION_LOG_SUM_EXP,
    DML_REDUCE_FUNCTION_MAX,
    DML_REDUCE_FUNCTION_MIN,
    DML_REDUCE_FUNCTION_MULTIPLY,
    DML_REDUCE_FUNCTION_SUM,
    DML_REDUCE_FUNCTION_SUM_SQUARE,
};

enum DML_MATRIX_TRANSFORM
{
    DML_MATRIX_TRANSFORM_NONE,
    DML_MATRIX_TRANSFORM_TRANSPOSE,
};

enum DML_CONVOLUTION_MODE
{
    DML_CONVOLUTION_MODE_CONVOLUTION,
    DML_CONVOLUTION_MODE_CROSS_CORRELATION,
};

enum DML_CONVOLUTION_DIRECTION
{
    DML_CONVOLUTION_DIRECTION_FORWARD,
    DML_CONVOLUTION_DIRECTION_BACKWARD,
};

enum DML_PADDING_MODE
{
    DML_PADDING_MODE_CONSTANT,
    DML_PADDING_MODE_EDGE,
    DML_PADDING_MODE_REFLECTION,
};

enum DML_INTERPOLATION_MODE
{
    DML_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
    DML_INTERPOLATION_MODE_LINEAR,
};

struct DML_SCALE_BIAS
{
    FLOAT Scale;
    FLOAT Bias;
};

struct DML_SIZE_2D
{
    UINT Width;
    UINT Height;
};

enum DML_RECURRENT_NETWORK_DIRECTION
{
    DML_RECURRENT_NETWORK_DIRECTION_FORWARD,
    DML_RECURRENT_NETWORK_DIRECTION_BACKWARD,
    DML_RECURRENT_NETWORK_DIRECTION_BIDIRECTIONAL,
};

// ===================================================================================================================
//   Operator descriptions
// ===================================================================================================================

struct DML_OPERATOR_DESC
{
    DML_OPERATOR_TYPE Type;
    _In_reads_(_Inexpressible_("Dependent on operator type")) const void* Desc;
};

struct DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
};

struct DML_ELEMENT_WISE_ABS_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
};

struct DML_ELEMENT_WISE_ACOS_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
};

struct DML_ELEMENT_WISE_ADD_OPERATOR_DESC
{
    const DML_TENSOR_DESC* ATensor;
    const DML_TENSOR_DESC* BTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ELEMENT_WISE_ASIN_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
};

struct DML_ELEMENT_WISE_ATAN_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
};

struct DML_ELEMENT_WISE_CEIL_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
};

struct DML_ELEMENT_WISE_CLIP_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
    FLOAT Min;
    FLOAT Max;
};

struct DML_ELEMENT_WISE_COS_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
};

struct DML_ELEMENT_WISE_DIVIDE_OPERATOR_DESC
{
    const DML_TENSOR_DESC* ATensor;
    const DML_TENSOR_DESC* BTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ELEMENT_WISE_EXP_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
};

struct DML_ELEMENT_WISE_FLOOR_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
};

struct DML_ELEMENT_WISE_LOG_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
};

struct DML_ELEMENT_WISE_LOGICAL_AND_OPERATOR_DESC
{
    const DML_TENSOR_DESC* ATensor;
    const DML_TENSOR_DESC* BTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ELEMENT_WISE_LOGICAL_EQUALS_OPERATOR_DESC
{
    const DML_TENSOR_DESC* ATensor;
    const DML_TENSOR_DESC* BTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ELEMENT_WISE_LOGICAL_GREATER_THAN_OPERATOR_DESC
{
    const DML_TENSOR_DESC* ATensor;
    const DML_TENSOR_DESC* BTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ELEMENT_WISE_LOGICAL_LESS_THAN_OPERATOR_DESC
{
    const DML_TENSOR_DESC* ATensor;
    const DML_TENSOR_DESC* BTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ELEMENT_WISE_LOGICAL_NOT_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ELEMENT_WISE_LOGICAL_OR_OPERATOR_DESC
{
    const DML_TENSOR_DESC* ATensor;
    const DML_TENSOR_DESC* BTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ELEMENT_WISE_LOGICAL_XOR_OPERATOR_DESC
{
    const DML_TENSOR_DESC* ATensor;
    const DML_TENSOR_DESC* BTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ELEMENT_WISE_MAX_OPERATOR_DESC
{
    const DML_TENSOR_DESC* ATensor;
    const DML_TENSOR_DESC* BTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ELEMENT_WISE_MEAN_OPERATOR_DESC
{
    const DML_TENSOR_DESC* ATensor;
    const DML_TENSOR_DESC* BTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ELEMENT_WISE_MIN_OPERATOR_DESC
{
    const DML_TENSOR_DESC* ATensor;
    const DML_TENSOR_DESC* BTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC
{
    const DML_TENSOR_DESC* ATensor;
    const DML_TENSOR_DESC* BTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ELEMENT_WISE_POW_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* ExponentTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
};

struct DML_ELEMENT_WISE_CONSTANT_POW_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
    FLOAT Exponent;
};

struct DML_ELEMENT_WISE_RECIP_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
};

struct DML_ELEMENT_WISE_SIN_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
};

struct DML_ELEMENT_WISE_SQRT_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
};

struct DML_ELEMENT_WISE_SUBTRACT_OPERATOR_DESC
{
    const DML_TENSOR_DESC* ATensor;
    const DML_TENSOR_DESC* BTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ELEMENT_WISE_TAN_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
};

struct DML_ELEMENT_WISE_THRESHOLD_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    _In_opt_ const DML_SCALE_BIAS* ScaleBias;
    FLOAT Min;
};

struct DML_ELEMENT_WISE_QUANTIZE_LINEAR_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* ScaleTensor;
    const DML_TENSOR_DESC* ZeroPointTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ELEMENT_WISE_DEQUANTIZE_LINEAR_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* ScaleTensor;
    const DML_TENSOR_DESC* ZeroPointTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ACTIVATION_ELU_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    FLOAT Alpha;
};

struct DML_ACTIVATION_HARDMAX_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ACTIVATION_HARD_SIGMOID_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    FLOAT Alpha;
    FLOAT Beta;
};

struct DML_ACTIVATION_IDENTITY_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ACTIVATION_LEAKY_RELU_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    FLOAT Alpha;
};

struct DML_ACTIVATION_LINEAR_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    FLOAT Alpha;
    FLOAT Beta;
};

struct DML_ACTIVATION_LOG_SOFTMAX_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ACTIVATION_PARAMETERIZED_RELU_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* SlopeTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ACTIVATION_PARAMETRIC_SOFTPLUS_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    FLOAT Alpha;
    FLOAT Beta;
};

struct DML_ACTIVATION_RELU_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ACTIVATION_SCALED_ELU_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    FLOAT Alpha;
    FLOAT Gamma;
};

struct DML_ACTIVATION_SCALED_TANH_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    FLOAT Alpha;
    FLOAT Beta;
};

struct DML_ACTIVATION_SIGMOID_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ACTIVATION_SOFTMAX_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ACTIVATION_SOFTPLUS_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    FLOAT Steepness;
};

struct DML_ACTIVATION_SOFTSIGN_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ACTIVATION_TANH_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_ACTIVATION_THRESHOLDED_RELU_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    FLOAT Alpha;
};

struct DML_CONVOLUTION_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* FilterTensor;
    _In_opt_ const DML_TENSOR_DESC* BiasTensor;
    const DML_TENSOR_DESC* OutputTensor;
    DML_CONVOLUTION_MODE Mode;
    DML_CONVOLUTION_DIRECTION Direction;
    UINT DimensionCount;
    _In_reads_(DimensionCount) const UINT* Strides;
    _In_reads_(DimensionCount) const UINT* Dilations;
    _In_reads_(DimensionCount) const UINT* StartPadding;
    _In_reads_(DimensionCount) const UINT* EndPadding;
    _In_reads_(DimensionCount) const UINT* OutputPadding;
    UINT GroupCount;
    _In_opt_ const DML_OPERATOR_DESC* FusedActivation;
};

struct DML_GEMM_OPERATOR_DESC
{
    const DML_TENSOR_DESC* ATensor;
    const DML_TENSOR_DESC* BTensor;
    _In_opt_ const DML_TENSOR_DESC* CTensor;
    const DML_TENSOR_DESC* OutputTensor;
    DML_MATRIX_TRANSFORM TransA;
    DML_MATRIX_TRANSFORM TransB;
    FLOAT Alpha;
    FLOAT Beta;
    _In_opt_ const DML_OPERATOR_DESC* FusedActivation;
};

struct DML_REDUCE_OPERATOR_DESC
{
    DML_REDUCE_FUNCTION Function;
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    UINT AxisCount;
    _In_reads_(AxisCount) const UINT* Axes;
};

struct DML_AVERAGE_POOLING_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    UINT DimensionCount;
    _In_reads_(DimensionCount) const UINT* Strides;
    _In_reads_(DimensionCount) const UINT* WindowSize;
    _In_reads_(DimensionCount) const UINT* StartPadding;
    _In_reads_(DimensionCount) const UINT* EndPadding;
    BOOL IncludePadding;
};

struct DML_LP_POOLING_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    UINT DimensionCount;
    _In_reads_(DimensionCount) const UINT* Strides;
    _In_reads_(DimensionCount) const UINT* WindowSize;
    _In_reads_(DimensionCount) const UINT* StartPadding;
    _In_reads_(DimensionCount) const UINT* EndPadding;
    UINT P;
};

struct DML_MAX_POOLING_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    UINT DimensionCount;
    _In_reads_(DimensionCount) const UINT* Strides;
    _In_reads_(DimensionCount) const UINT* WindowSize;
    _In_reads_(DimensionCount) const UINT* StartPadding;
    _In_reads_(DimensionCount) const UINT* EndPadding;
};

struct DML_ROI_POOLING_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* ROITensor;
    const DML_TENSOR_DESC* OutputTensor;
    FLOAT SpatialScale;
    DML_SIZE_2D PooledSize;
};

struct DML_SLICE_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    UINT DimensionCount;
    _In_reads_(DimensionCount) const UINT* Offsets;
    _In_reads_(DimensionCount) const UINT* Sizes;
    _In_reads_(DimensionCount) const UINT* Strides;
};

struct DML_CAST_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
};

struct DML_SPLIT_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    UINT OutputCount;
    _In_reads_(OutputCount) const DML_TENSOR_DESC* OutputTensors;
    UINT Axis;
};

struct DML_JOIN_OPERATOR_DESC
{
    UINT InputCount;
    _In_reads_(InputCount) const DML_TENSOR_DESC* InputTensors;
    const DML_TENSOR_DESC* OutputTensor;
    UINT Axis;
};

struct DML_PADDING_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    DML_PADDING_MODE PaddingMode;
    FLOAT PaddingValue;
    UINT DimensionCount;
    _In_reads_(DimensionCount) const UINT* StartPadding;
    _In_reads_(DimensionCount) const UINT* EndPadding;
};

struct DML_VALUE_SCALE_2D_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    FLOAT Scale;
    UINT ChannelCount;
    _In_reads_(ChannelCount) const FLOAT* Bias;
};

struct DML_UPSAMPLE_2D_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    DML_SIZE_2D ScaleSize;
    DML_INTERPOLATION_MODE InterpolationMode;
};

struct DML_GATHER_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* IndicesTensor;
    const DML_TENSOR_DESC* OutputTensor;
    UINT Axis;
    UINT IndexDimensions;
};

struct DML_SPACE_TO_DEPTH_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    UINT BlockSize;
};

struct DML_DEPTH_TO_SPACE_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    UINT BlockSize;
};

struct DML_TILE_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    UINT RepeatsCount;
    _In_reads_(RepeatsCount) const UINT* Repeats;
};

struct DML_TOP_K_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputValueTensor;
    const DML_TENSOR_DESC* OutputIndexTensor;
    UINT Axis;
    UINT K;
};

struct DML_BATCH_NORMALIZATION_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* MeanTensor;
    const DML_TENSOR_DESC* VarianceTensor;
    const DML_TENSOR_DESC* ScaleTensor;
    const DML_TENSOR_DESC* BiasTensor;
    const DML_TENSOR_DESC* OutputTensor;
    BOOL Spatial;
    FLOAT Epsilon;
    _In_opt_ const DML_OPERATOR_DESC* FusedActivation;
};

struct DML_MEAN_VARIANCE_NORMALIZATION_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    _In_opt_ const DML_TENSOR_DESC* ScaleTensor;
    _In_opt_ const DML_TENSOR_DESC* BiasTensor;
    const DML_TENSOR_DESC* OutputTensor;
    BOOL CrossChannel;
    BOOL NormalizeVariance;
    FLOAT Epsilon;
    _In_opt_ const DML_OPERATOR_DESC* FusedActivation;
};

struct DML_LOCAL_RESPONSE_NORMALIZATION_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    BOOL CrossChannel;
    UINT LocalSize;
    FLOAT Alpha;
    FLOAT Beta;
    FLOAT Bias;
};

struct DML_LP_NORMALIZATION_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* OutputTensor;
    UINT Axis;
    FLOAT Epsilon;
    UINT P;
};

struct DML_RNN_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* WeightTensor;
    const DML_TENSOR_DESC* RecurrenceTensor;
    _In_opt_ const DML_TENSOR_DESC* BiasTensor;
    _In_opt_ const DML_TENSOR_DESC* HiddenInitTensor;
    _In_opt_ const DML_TENSOR_DESC* SequenceLengthsTensor;
    _In_opt_ const DML_TENSOR_DESC* OutputSequenceTensor;
    _In_opt_ const DML_TENSOR_DESC* OutputSingleTensor;
    UINT ActivationDescCount;
    _In_reads_(ActivationDescCount) const DML_OPERATOR_DESC* ActivationDescs;
    DML_RECURRENT_NETWORK_DIRECTION Direction;
};

struct DML_LSTM_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* WeightTensor;
    const DML_TENSOR_DESC* RecurrenceTensor;
    _In_opt_ const DML_TENSOR_DESC* BiasTensor;
    _In_opt_ const DML_TENSOR_DESC* HiddenInitTensor;
    _In_opt_ const DML_TENSOR_DESC* CellMemInitTensor;
    _In_opt_ const DML_TENSOR_DESC* SequenceLengthsTensor;
    _In_opt_ const DML_TENSOR_DESC* PeepholeTensor;
    _In_opt_ const DML_TENSOR_DESC* OutputSequenceTensor;
    _In_opt_ const DML_TENSOR_DESC* OutputSingleTensor;
    _In_opt_ const DML_TENSOR_DESC* OutputCellSingleTensor;
    UINT ActivationDescCount;
    _In_reads_(ActivationDescCount) const DML_OPERATOR_DESC* ActivationDescs;
    DML_RECURRENT_NETWORK_DIRECTION Direction;
    float ClipThreshold;
    BOOL UseClipThreshold;
    BOOL CoupleInputForget;
};

struct DML_GRU_OPERATOR_DESC
{
    const DML_TENSOR_DESC* InputTensor;
    const DML_TENSOR_DESC* WeightTensor;
    const DML_TENSOR_DESC* RecurrenceTensor;
    _In_opt_ const DML_TENSOR_DESC* BiasTensor;
    _In_opt_ const DML_TENSOR_DESC* HiddenInitTensor;
    _In_opt_ const DML_TENSOR_DESC* SequenceLengthsTensor;
    _In_opt_ const DML_TENSOR_DESC* OutputSequenceTensor;
    _In_opt_ const DML_TENSOR_DESC* OutputSingleTensor;
    UINT ActivationDescCount;
    _In_reads_(ActivationDescCount) const DML_OPERATOR_DESC* ActivationDescs;
    DML_RECURRENT_NETWORK_DIRECTION Direction;
    BOOL LinearBeforeReset;
};

// ===================================================================================================================
//   DML feature support queries
// ===================================================================================================================

enum DML_FEATURE
{
    DML_FEATURE_TENSOR_DATA_TYPE_SUPPORT,
};

struct DML_FEATURE_QUERY_TENSOR_DATA_TYPE_SUPPORT
{
    DML_TENSOR_DATA_TYPE DataType;
};

struct DML_FEATURE_DATA_TENSOR_DATA_TYPE_SUPPORT
{
    BOOL IsSupported;
};


// ===================================================================================================================
//   DML device functions, enumerations, and structures
// ===================================================================================================================

struct DML_BINDING_TABLE_DESC
{
    IDMLDispatchable* Dispatchable;
    D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptorHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptorHandle;
    UINT SizeInDescriptors;
};

enum DML_EXECUTION_FLAGS
{
    DML_EXECUTION_FLAG_NONE = 0,
    DML_EXECUTION_FLAG_ALLOW_HALF_PRECISION_COMPUTATION = 0x1,
    DML_EXECUTION_FLAG_DISABLE_META_COMMANDS = 0x2,
    DML_EXECUTION_FLAG_DESCRIPTORS_VOLATILE = 0x4,
};

DEFINE_ENUM_FLAG_OPERATORS(DML_EXECUTION_FLAGS)

enum DML_CREATE_DEVICE_FLAGS
{
    DML_CREATE_DEVICE_FLAG_NONE = 0,
    DML_CREATE_DEVICE_FLAG_DEBUG = 0x1,
};

DEFINE_ENUM_FLAG_OPERATORS(DML_CREATE_DEVICE_FLAGS)

STDAPI DMLCreateDevice(
    ID3D12Device* d3d12Device,
    DML_CREATE_DEVICE_FLAGS flags,
    REFIID riid, // Expected: IDMLDevice
    _COM_Outptr_opt_ void** device
    );


// ===================================================================================================================
//   DML object
// ===================================================================================================================

interface DML_DECLARE_INTERFACE("c8263aac-9e0c-4a2d-9b8e-007521a3317c") IDMLObject : IUnknown
{
    IFACEMETHOD(GetPrivateData)(
        REFGUID guid,
        _Inout_ UINT* dataSize,
        _Out_writes_bytes_opt_(*dataSize) void* data
        ) = 0;

    IFACEMETHOD(SetPrivateData)(
        REFGUID guid,
        UINT dataSize,
        _In_reads_bytes_opt_(dataSize) const void* data
        ) = 0;

    IFACEMETHOD(SetPrivateDataInterface)(
        REFGUID guid,
        _In_opt_ IUnknown* data
        ) = 0;

    IFACEMETHOD(SetName)(
        PCWSTR name
        ) = 0;
};

// ===================================================================================================================
//   DML device
// ===================================================================================================================

interface DML_DECLARE_INTERFACE("6dbd6437-96fd-423f-a98c-ae5e7c2a573f") IDMLDevice : IDMLObject
{
    IFACEMETHOD(CheckFeatureSupport)(
        DML_FEATURE feature,
        UINT featureQueryDataSize,
        _In_reads_bytes_opt_(featureQueryDataSize) const void* featureQueryData,
        UINT featureSupportDataSize,
        _Out_writes_bytes_(featureSupportDataSize) void* featureSupportData
        ) = 0;
    
    IFACEMETHOD(CreateOperator)(
        const DML_OPERATOR_DESC* desc,
        REFIID riid, // expected: IDMLOperator
        _COM_Outptr_opt_ void** ppv
        ) = 0;
    
    IFACEMETHOD(CompileOperator)(
        IDMLOperator* op,
        DML_EXECUTION_FLAGS flags,
        REFIID riid, // expected: IDMLCompiledOperator
        _COM_Outptr_opt_ void** ppv
        ) = 0;
    
    IFACEMETHOD(CreateOperatorInitializer)(
        UINT operatorCount,
        _In_reads_opt_(operatorCount) IDMLCompiledOperator* const* operators,
        REFIID riid, // expected: IDMLOperatorInitializer
        _COM_Outptr_ void** ppv
        ) = 0;
    
    IFACEMETHOD(CreateCommandRecorder)(
        REFIID riid, // expected: IDMLCommandRecorder
        _COM_Outptr_ void** ppv
        ) = 0;
    
    IFACEMETHOD(CreateBindingTable)(
        _In_opt_ const DML_BINDING_TABLE_DESC* desc,
        REFIID riid, // expected: IDMLBindingTable
        _COM_Outptr_ void** ppv
        ) = 0;
    
    IFACEMETHOD(Evict)(
        UINT count,
        _In_reads_(count) IDMLPageable* const* ppObjects
        ) = 0;
    
    IFACEMETHOD(MakeResident)(
        UINT count,
        _In_reads_(count) IDMLPageable* const* ppObjects
        ) = 0;
    
    IFACEMETHOD(GetDeviceRemovedReason)(
        ) = 0;

    IFACEMETHOD(GetParentDevice)(
        REFIID riid,
        _COM_Outptr_ void** ppv
        ) = 0;
};


// ===================================================================================================================
//   DML device children
// ===================================================================================================================

interface DML_DECLARE_INTERFACE("27e83142-8165-49e3-974e-2fd66e4cb69d") IDMLDeviceChild : IDMLObject
{
    IFACEMETHOD(GetDevice)(
        REFIID riid, // expected: IDMLDevice
        _COM_Outptr_ void** ppv
        ) = 0;
};

interface DML_DECLARE_INTERFACE("b1ab0825-4542-4a4b-8617-6dde6e8f6201") IDMLPageable : IDMLDeviceChild
{
};


// ===================================================================================================================
//   DML operator
// ===================================================================================================================

interface DML_DECLARE_INTERFACE("26caae7a-3081-4633-9581-226fbe57695d") IDMLOperator : IDMLDeviceChild
{
};


// ===================================================================================================================
//   DML dispatchable
// ===================================================================================================================

struct DML_BINDING_PROPERTIES
{
    UINT RequiredDescriptorCount;
    UINT64 TemporaryResourceSize;
    UINT64 PersistentResourceSize;
};

interface DML_DECLARE_INTERFACE("dcb821a8-1039-441e-9f1c-b1759c2f3cec") IDMLDispatchable : IDMLPageable
{
    IFACEMETHOD_(DML_BINDING_PROPERTIES, GetBindingProperties)() = 0;
};


// ===================================================================================================================
//   DML compiled operator
// ===================================================================================================================

interface DML_DECLARE_INTERFACE("6b15e56a-bf5c-4902-92d8-da3a650afea4") IDMLCompiledOperator : IDMLDispatchable
{
};


// ===================================================================================================================
//   DML operator initializer
// ===================================================================================================================

interface DML_DECLARE_INTERFACE("427c1113-435c-469c-8676-4d5dd072f813") IDMLOperatorInitializer : IDMLDispatchable
{
    IFACEMETHOD(Reset)(
        UINT operatorCount,
        _In_reads_opt_(operatorCount) IDMLCompiledOperator* const* operators
        ) = 0;
};

// ===================================================================================================================
//   DML binding table
// ===================================================================================================================

enum DML_BINDING_TYPE
{
    DML_BINDING_TYPE_NONE,
    DML_BINDING_TYPE_BUFFER,
    DML_BINDING_TYPE_BUFFER_ARRAY,
};

struct DML_BINDING_DESC
{
    DML_BINDING_TYPE Type;
    _In_reads_opt_(_Inexpressible_("Dependent on binding type")) const void* Desc;
};

struct DML_BUFFER_BINDING
{
    _In_opt_ ID3D12Resource* Buffer;
    UINT64 Offset;
    UINT64 SizeInBytes;
};

struct DML_BUFFER_ARRAY_BINDING
{
    UINT BindingCount;
    _In_reads_(BindingCount) const DML_BUFFER_BINDING* Bindings;
};

interface DML_DECLARE_INTERFACE("29c687dc-de74-4e3b-ab00-1168f2fc3cfc") IDMLBindingTable : IDMLDeviceChild
{
    IFACEMETHOD_(void, BindInputs)(
        UINT bindingCount,
        _In_reads_opt_(bindingCount) const DML_BINDING_DESC* bindings
        ) = 0;

    IFACEMETHOD_(void, BindOutputs)(
        UINT bindingCount,
        _In_reads_opt_(bindingCount) const DML_BINDING_DESC* bindings
        ) = 0;

    IFACEMETHOD_(void, BindTemporaryResource)(
        _In_opt_ const DML_BINDING_DESC* binding
        ) = 0;

    IFACEMETHOD_(void, BindPersistentResource)(
        _In_opt_ const DML_BINDING_DESC* binding
        ) = 0;

    IFACEMETHOD(Reset)(
        _In_opt_ const DML_BINDING_TABLE_DESC* desc
        ) = 0;
};


// ===================================================================================================================
//   DML command recorder
// ===================================================================================================================

interface DML_DECLARE_INTERFACE("e6857a76-2e3e-4fdd-bff4-5d2ba10fb453") IDMLCommandRecorder : IDMLDeviceChild
{
    IFACEMETHOD_(void, RecordDispatch)(
        ID3D12CommandList* commandList,
        IDMLDispatchable* dispatchable,
        IDMLBindingTable* bindings
        ) = 0;
};


// ===================================================================================================================
//   DML debug
// ===================================================================================================================

interface DML_DECLARE_INTERFACE("7d6f3ac9-394a-4ac3-92a7-390cc57a8217") IDMLDebugDevice : IUnknown
{
    IFACEMETHOD_(void, SetMuteDebugOutput)(
        BOOL mute
        ) = 0;
};

#endif // WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP | WINAPI_PARTITION_GAMES)
#endif // DIRECTML_H
