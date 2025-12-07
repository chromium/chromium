# Quantization
TFLite's representation for quantization differs from WebNN. The quantization information is attached to the tensor itself instead of being represented as explicit `quantizeLinear` and `dequantizeLinear` operations. TFLite does support `quantize` and `dequantize` operations but using them explicitly is not as efficient as representing the graph with quantized tensors. This lets each operation kernel to decide whether it can operate on the quantized values directly or needs to dequantize back to floats.

Example WebNN graph representing computation the developer intends to be performed with quantized int8 precision:

`input [float32] -> quantizeLinear [int8] -> dequantizeLinear [float32] -> conv2d [float32] -> quantizeLinear [int8] -> dequantizeLinear [float32] -> avgPool2d [float32] -> quantizeLinear [int8] -> dequantizeLinear [float32]`

Ideal TFLite graph:

`input [float32] -> quantizeLinear [qint8] -> conv2d [qint8] -> avgPool2d [qint8] -> dequantizeLinear [float32]`

We try to pattern match such DQ->[op]->Q pair during TFLite graph building to fuse Q, DQ operations and attach quantization parameters accordingly. This is achieved by delaying the serialization of the `dequantizeLinear` operation until a subsequent operation that can't be fused is encountered. This can happen if the operation is not followed by a `quantizeLinear` op or the quantization parameters don't meet constraints that allow them to be represented by a TFLite model.

## Fusion criteria
We can only fuse the DQ->[op]->Q pair if this operation supports qint tensor types as inputs. This is [publicly documented](https://www.tensorflow.org/mlir/tfl_ops) - see a lot of operations have QI8 and QUI8 as supported input types.

However there are more undocumented constraints on the quantization parameters. For example, conv2d only allows QI8 inputs if [filter has all 0 zero-points quantization](https://source.chromium.org/chromium/chromium/src/+/main:third_party/TFLite/src/tensorflow/lite/kernels/conv.cc;l=363;drc=e433dac46a0bb8ffa4b6e600d4d94751768392c0). Currently WebNN implements all these validations until TFLite widens its support for more generic quantization schemes.

## Special handling for edge cases
### Edge case 1: mismatching quantization parameters.
In WebNN, users can specify a graph such as `input [float32] -> quantizeLinear [int8] -> dequantizeLinear [float32]` with different quantization parameters for `dequantizeLinear` and `quantizeLinear` This is not representable in TFLite as `input [float32] -> quantizeLinear [qint] -> dequantizeLinear [float32]` because the quantization parameters are attached to the `qint` tensor itself. So for such a case, the `dequantizeLinear` operation has to be decomposed.

### Edge case 2: dequantize from an operation's output without quantization parameters.
To serialize a WebNN graph: `input [int8] -> transpose [int8] -> dequantizeLinear [float32] -> ...`, when seeing `dequantizeLinear`, we try to attach the quantization parameters to its input operand, but then we find this operand is already serialized as the output of transpose, which doesn't have quantization parameters. To solve this, we try to trace back upstream operations from dequantizeLinear, if they are quantization agnostic operations(e.g. `reshape`, `transpose`) and their inputs and outputs can be attached with the quantization parameters for the `dequantizeLinear`, we attach all the way back to graph input/constant operands.
So the WebNN graph `input [int8] -> transpose [int8] -> dequantizeLinear [float32] -> ...`, becomes:
`input [qint8] -> transpose [qint8] -> dequantizeLinear [float32] -> ...`

If the upstream operation is not quantization agnostic, e.g. `add`, `mul`, we serialize the `dequantizeLinear` with decomposition because we can't guess what the correct quantization parameters for upstream operations' inputs are.

