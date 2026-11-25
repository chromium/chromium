// META: title=validation tests for WebNN API gatherElements operation
// META: global=window
// META: variant=?cpu
// META: variant=?gpu
// META: variant=?npu
// META: script=../resources/utils_validation.js

'use strict';

const tests = [
  {
    name: '[gatherElements] Test gatherElements with default options',
    input: {dataType: 'float32', shape: [1, 2, 3]},
    indices: {dataType: 'int32', shape: [2, 2, 3]},
    output: {dataType: 'float32', shape: [2, 2, 3]}
  },
  {
    name: '[gatherElements] Test gatherElements with axis = 2',
    input: {dataType: 'float32', shape: [1, 2, 3, 4]},
    indices: {dataType: 'int32', shape: [1, 2, 1, 4]},
    axis: 2,
    output: {dataType: 'float32', shape: [1, 2, 1, 4]}
  },
  {
    name: '[gatherElements] Throw if input is a scalar',
    input: {dataType: 'float32', shape: []},
    indices: {dataType: 'int32', shape: []}
  },
  {
    name:
        '[gatherElements] Throw if the axis is greater than the rank of input',
    input: {dataType: 'float32', shape: [1, 2, 3]},
    indices: {dataType: 'int32', shape: [1, 2, 3]},
    axis: 4
  },
  {
    name: '[gatherElements] Throw if indices data type is float32',
    input: {dataType: 'float32', shape: [1, 2, 3]},
    indices: {dataType: 'float32', shape: [1, 2, 3]}
  },
  {
    name: '[gatherElements] Throw if input rank is not equal to indices rank',
    input: {dataType: 'float32', shape: [1, 2, 3]},
    indices: {dataType: 'int32', shape: [1, 2]}
  },
  {
    name: '[gatherElements] Throw if indices shape is incorrect',
    input: {dataType: 'float32', shape: [1, 2, 3, 4]},
    indices: {dataType: 'int32', shape: [3, 2, 3, 4]},
    axis: 3
  }
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
        const output = builder.gatherElements(input, indices, options);
        assert_equals(output.dataType, test.output.dataType);
        assert_array_equals(output.shape, test.output.shape);
      } else {
        const label = 'gatherElements_'
        options.label = label;
        const regrexp = new RegExp('\\[' + label + '\\]');
        assert_throws_with_label(
            () => builder.gatherElements(input, indices, options), regrexp);
      }
    }, test.name));

multi_builder_test(async (t, builder, otherBuilder) => {
  const inputFromOtherBuilder =
      otherBuilder.input('input', {dataType: 'float32', shape: [2, 2]});

  const indices = builder.input('indices', {dataType: 'int32', shape: [2, 2]});
  assert_throws_js(
      TypeError, () => builder.gatherElements(inputFromOtherBuilder, indices));
}, '[gatherElements] Throw if input is from another builder');

multi_builder_test(async (t, builder, otherBuilder) => {
  const indicesFromOtherBuilder =
      otherBuilder.input('indices', {dataType: 'int32', shape: [2, 2]});

  const input = builder.input('input', {dataType: 'float32', shape: [2, 2]});
  assert_throws_js(
      TypeError, () => builder.gatherElements(input, indicesFromOtherBuilder));
}, '[gatherElements] Throw if indices is from another builder');

validateOperandRankOutOfRange('gatherElements', 'input', (builder, input) => {
  const indices = builder.input('indices', {
    dataType: 'int32',
    shape: Array(input.shape.length).fill(kExampleDimSize)
  });
  return builder.gatherElements(input, indices);
});

validateOperandRankOutOfRange(
    'gatherElements', 'indices', (builder, indices) => {
      // Per
      // https://webmachinelearning.github.io/webnn/#api-mlgraphbuilder-gatherelements,
      // indices's rank must equal input's rank, so input is built with the
      // same (out-of-range) rank as indices to isolate the indices rank
      // violation.
      const input = builder.input('input', {
        dataType: 'float32',
        shape: Array(indices.shape.length).fill(kExampleDimSize)
      });
      return builder.gatherElements(input, indices);
    });

// Cartesian iteration over (input rank, indices rank) pairs allowed by
// opSupportLimits. gatherElements requires indices.rank == input.rank, so
// only the diagonal is a valid combination. Uses uniform [kExampleDimSize]
// dims; all dims of `input` and `indices` match so no per-axis size mismatch.
promise_test(async t => {
  const opLimits = context.opSupportLimits().gatherElements;
  if (!opLimits)
    return;
  const {min: inputMin, max: inputMax} = opLimits.input.rankRange;
  const {min: indicesMin, max: indicesMax} = opLimits.indices.rankRange;
  const globalRankMax = getGlobalRankMax();
  const inputDataTypeInfo = pickSupportedDataTypeInfo(opLimits.input);
  const indicesDataTypeInfo = pickSupportedDataTypeInfo(opLimits.indices);
  if (!inputDataTypeInfo || !indicesDataTypeInfo)
    return;
  const rankMin = Math.max(inputMin, indicesMin);
  const rankMax = Math.min(inputMax, indicesMax);
  for (let rank = rankMin; rank <= rankMax; rank++) {
    const builder = new MLGraphBuilder(context);
    const input = buildInputOperand(builder, 'input', inputDataTypeInfo,
                                    Array(rank).fill(kExampleDimSize));
    const indices = buildInputOperand(builder, 'indices', indicesDataTypeInfo,
                                      Array(rank).fill(kExampleDimSize));
    const output = builder.gatherElements(input, indices);
    assert_true(
        output instanceof MLOperand,
        `(input rank ${rank}, indices rank ${rank}) should be accepted`);
  }
}, '[gatherElements] accept every (input rank, indices rank) pair allowed by opSupportLimits');
