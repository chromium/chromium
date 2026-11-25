// META: title=validation tests for WebNN API gatherND operation
// META: global=window
// META: variant=?cpu
// META: variant=?gpu
// META: variant=?npu
// META: script=../resources/utils_validation.js

'use strict';

const label = 'gatherND_';
const regexp = new RegExp('\\[' + label + '\\]');
const tests = [
  {
    name: '[gatherND] Test gatherND with 5D input 3D indices',
    input: {dataType: 'float32', shape: [2, 2, 3, 3, 4]},
    indices: {dataType: 'int32', shape: [5, 4, 3]},
    output: {dataType: 'float32', shape: [5, 4, 3, 4]}
  },
  {
    name: '[gatherND] Throw if input is a scalar',
    input: {dataType: 'float32', shape: []},
    indices: {dataType: 'int32', shape: [1, 1, 1]}
  },
  {
    name: '[gatherND] Throw if indices is a scalar',
    input: {dataType: 'float32', shape: [1, 1, 1]},
    indices: {dataType: 'int32', shape: []}
  },
  {
    name: '[gatherND] Throw if indices data type is float32',
    input: {dataType: 'float32', shape: [1, 2, 3]},
    indices: {dataType: 'float32', shape: [1, 1, 1]},
  },
  {
    name:
        '[gatherND] Throw if indices.shape[-1] is greater than the input rank',
    input: {dataType: 'float32', shape: [1, 2, 3]},
    indices: {dataType: 'int32', shape: [1, 1, 4]}
  }
];

tests.forEach(test => promise_test(async t => {
                const builder = new MLGraphBuilder(context);
                const input = builder.input('input', test.input);
                const indices = builder.input('indices', test.indices);

                if (test.output &&
                    context.opSupportLimits().gatherND.input.dataTypes.includes(
                        test.input.dataType)) {
                  const output = builder.gatherND(input, indices);
                  assert_equals(output.dataType, test.output.dataType);
                  assert_array_equals(output.shape, test.output.shape);
                } else {
                  const options = {label: label};
                  assert_throws_with_label(
                      () => builder.gatherND(input, indices, options), regexp);
                }
              }, test.name));

multi_builder_test(async (t, builder, otherBuilder) => {
  const inputFromOtherBuilder =
      otherBuilder.input('input', {dataType: 'float32', shape: [2, 2]});

  const indices = builder.input('indices', {dataType: 'int32', shape: [2, 1]});
  assert_throws_js(
      TypeError, () => builder.gatherND(inputFromOtherBuilder, indices));
}, '[gatherND] Throw if input is from another builder');

multi_builder_test(async (t, builder, otherBuilder) => {
  const indicesFromOtherBuilder =
      otherBuilder.input('indices', {dataType: 'int32', shape: [2, 2]});

  const input = builder.input('input', {dataType: 'float32', shape: [2, 1]});
  assert_throws_js(
      TypeError, () => builder.gatherND(input, indicesFromOtherBuilder));
}, '[gatherND] Throw if indices is from another builder');

promise_test(async t => {
  const builder = new MLGraphBuilder(context);

  const input = builder.input('input', {
      dataType: 'float32', shape: [2, 2, 3, 3, 4]});
  const indices = builder.input('indices', {
    dataType: 'int32',
    shape: [context.opSupportLimits().maxTensorByteLength / 4, 1, 1]});

  const options = {label};
  assert_throws_with_label(
      () => builder.gatherND(input, indices, options), regexp);
}, '[gatherND] throw if the output tensor byte length exceeds limit');

validateOperandRankOutOfRange('gatherND', 'input', (builder, input) => {
  const indices = builder.input(
      'indices', {dataType: 'int32', shape: [kExampleDimSize, 1]});
  return builder.gatherND(input, indices);
});

validateOperandRankOutOfRange('gatherND', 'indices', (builder, indices) => {
  const input =
      builder.input('input', {dataType: 'float32', shape: [kExampleDimSize]});
  return builder.gatherND(input, indices);
});

// Cartesian iteration over (input rank, indices rank) pairs allowed by
// opSupportLimits. Fixes indices.shape[-1] = 1 so any input rank >= 1 is
// compatible; the remaining indices dims and all input dims are
// `kExampleDimSize`. Skips pairs whose output rank would exceed the output
// rankRange max.
promise_test(async t => {
  const opLimits = context.opSupportLimits().gatherND;
  if (!opLimits)
    return;
  const {min: inputMin, max: inputMax} = opLimits.input.rankRange;
  const {min: indicesMin, max: indicesMax} = opLimits.indices.rankRange;
  const inputDataTypeInfo = pickSupportedDataTypeInfo(opLimits.input);
  const indicesDataTypeInfo = pickSupportedDataTypeInfo(opLimits.indices);
  if (!inputDataTypeInfo || !indicesDataTypeInfo)
    return;
  const {max: outputRankMax} = opLimits.output.rankRange;
  for (let inputRank = Math.max(inputMin, 1); inputRank <= inputMax;
       inputRank++) {
    for (let indicesRank = Math.max(indicesMin, 1); indicesRank <= indicesMax;
         indicesRank++) {
      // With indices.shape[-1] = 1, output rank = indices rank - 1 +
      // input rank - 1. Skip pairs whose output rank would exceed the
      // global max.
      if (indicesRank - 1 + inputRank - 1 > outputRankMax)
        continue;
      // indices shape: [kExampleDimSize, ..., kExampleDimSize, 1] (last dim
      // = 1 so any input rank >= 1 is compatible).
      const indicesShape = Array(indicesRank).fill(kExampleDimSize);
      indicesShape[indicesRank - 1] = 1;
      const builder = new MLGraphBuilder(context);
      const input = buildInputOperand(builder, 'input', inputDataTypeInfo,
                                      Array(inputRank).fill(kExampleDimSize));
      const indices = buildInputOperand(builder, 'indices', indicesDataTypeInfo,
                                        indicesShape);
      const output = builder.gatherND(input, indices);
      assert_true(output instanceof MLOperand,
                  `(input rank ${inputRank}, indices rank ${
                      indicesRank}) should be accepted`);
    }
  }
}, '[gatherND] accept every (input rank, indices rank) pair allowed by opSupportLimits');
