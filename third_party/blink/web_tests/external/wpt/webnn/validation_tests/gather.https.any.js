// META: title=validation tests for WebNN API gather operation
// META: global=window
// META: variant=?cpu
// META: variant=?gpu
// META: variant=?npu
// META: script=../resources/utils_validation.js

'use strict';

const label = 'gather_'
const regrexp = new RegExp('\\[' + label + '\\]');
const tests = [
  {
    name: '[gather] Test gather with default options and 0-D indices',
    input: {dataType: 'int32', shape: [3]},
    indices: {dataType: 'int64', shape: []},
    output: {dataType: 'int32', shape: []}
  },
  {
    name: '[gather] Test gather with axis = 2',
    input: {dataType: 'float32', shape: [1, 2, 3, 4]},
    indices: {dataType: 'int64', shape: [5, 6]},
    axis: 2,
    output: {dataType: 'float32', shape: [1, 2, 5, 6, 4]}
  },
  {
    name: '[gather] Test gather with indices\'s dataType = uint32',
    input: {dataType: 'float32', shape: [1, 2, 3, 4]},
    indices: {dataType: 'uint32', shape: [5, 6]},
    axis: 2,
    output: {dataType: 'float32', shape: [1, 2, 5, 6, 4]}
  },
  {
    name: '[gather] Test gather with indices\'s dataType = int32',
    input: {dataType: 'float32', shape: [1, 2, 3, 4]},
    indices: {dataType: 'int32', shape: [5, 6]},
    axis: 2,
    output: {dataType: 'float32', shape: [1, 2, 5, 6, 4]}
  },
  {
    name: '[gather] TypeError is expected if the input is a scalar',
    input: {dataType: 'float16', shape: []},
    indices: {dataType: 'int64', shape: [1]},
  },
  {
    name:
        '[gather] TypeError is expected if the axis is greater than the rank of input',
    input: {dataType: 'float16', shape: [1, 2, 3]},
    indices: {dataType: 'int32', shape: [5, 6]},
    axis: 4,
  },
  {
    name:
        '[gather] TypeError is expected if the data type of indices is float32 which is invalid',
    input: {dataType: 'float16', shape: [1, 2, 3, 4]},
    indices: {dataType: 'float32', shape: [5, 6]},
  },
  {
    name:
        '[gather] TypeError is expected if the data type of indices is uint64 which is invalid',
    input: {dataType: 'float16', shape: [1, 2, 3, 4]},
    indices: {dataType: 'uint64', shape: [5, 6]},
  },
];

tests.forEach(
    test => promise_test(async t => {
      const builder = new MLGraphBuilder(context);
      const input = builder.input('input', test.input);
      const indices = builder.input('indices', test.indices);

      const options = {};
      if (test.axis) {
        options.axis = test.axis;
      }

      if (test.output) {
        const output = builder.gather(input, indices, options);
        assert_equals(output.dataType, test.output.dataType);
        assert_array_equals(output.shape, test.output.shape);
      } else {
        options.label = label;
        assert_throws_with_label(
            () => builder.gather(input, indices, options), regrexp);
      }
    }, test.name));

multi_builder_test(async (t, builder, otherBuilder) => {
  const inputFromOtherBuilder =
      otherBuilder.input('input', {dataType: 'float32', shape: [2, 2]});

  const indices = builder.input('indices', {dataType: 'int64', shape: [2, 2]});
  assert_throws_js(
      TypeError, () => builder.gather(inputFromOtherBuilder, indices));
}, '[gather] throw if input is from another builder');

multi_builder_test(async (t, builder, otherBuilder) => {
  const indicesFromOtherBuilder =
      otherBuilder.input('indices', {dataType: 'int64', shape: [2, 2]});

  const input = builder.input('input', {dataType: 'float32', shape: [2, 2]});
  assert_throws_js(
      TypeError, () => builder.gather(input, indicesFromOtherBuilder));
}, '[gather] throw if indices is from another builder');

promise_test(async t => {
  const builder = new MLGraphBuilder(context);

  const input = builder.input('input', {
      dataType: 'float32', shape: [1, 3, 3, 4]});
  const indices = builder.input('indices', {
      dataType: 'int32',
      shape: [context.opSupportLimits().maxTensorByteLength / 4] });

  const options = {};
  options.label = label;
  options.axis = 2;
  assert_throws_with_label(
      () => builder.gather(input, indices, options), regrexp);
}, '[gather] throw if the output tensor byte length exceeds limit');

validateOperandRankOutOfRange('gather', 'input', (builder, input) => {
  const indices =
      builder.input('indices', {dataType: 'int32', shape: [kExampleDimSize]});
  return builder.gather(input, indices);
});

validateOperandRankOutOfRange('gather', 'indices', (builder, indices) => {
  const input =
      builder.input('input', {dataType: 'float32', shape: [kExampleDimSize]});
  return builder.gather(input, indices);
});

// Cartesian iteration over (input rank, indices rank) pairs allowed by
// opSupportLimits. Uses uniform [kExampleDimSize] dims; skips pairs whose
// resulting output rank (= input.rank + indices.rank - 1, with default
// axis = 0) would exceed the output rankRange max.
promise_test(async t => {
  const opLimits = context.opSupportLimits().gather;
  if (!opLimits)
    return;
  const {min: inputMin, max: inputMax} = opLimits.input.rankRange;
  const {min: indicesMin, max: indicesMax} = opLimits.indices.rankRange;
  const inputDataTypeInfo = pickSupportedDataTypeInfo(opLimits.input);
  const indicesDataTypeInfo = pickSupportedDataTypeInfo(opLimits.indices);
  if (!inputDataTypeInfo || !indicesDataTypeInfo)
    return;
  const {max: outputRankMax} = opLimits.output.rankRange;
  for (let inputRank = inputMin; inputRank <= inputMax; inputRank++) {
    for (let indicesRank = indicesMin; indicesRank <= indicesMax;
         indicesRank++) {
      // With the default axis = 0, output rank = input rank - 1 + indices
      // rank. Skip pairs whose output rank would exceed the global max.
      if (inputRank - 1 + indicesRank > outputRankMax)
        continue;
      const builder = new MLGraphBuilder(context);
      const input = buildInputOperand(builder, 'input', inputDataTypeInfo,
                                      Array(inputRank).fill(kExampleDimSize));
      const indices =
          buildInputOperand(builder, 'indices', indicesDataTypeInfo,
                            Array(indicesRank).fill(kExampleDimSize));
      const output = builder.gather(input, indices);
      assert_true(output instanceof MLOperand,
                  `(input rank ${inputRank}, indices rank ${
                      indicesRank}) should be accepted`);
    }
  }
}, '[gather] accept every (input rank, indices rank) pair allowed by opSupportLimits');
