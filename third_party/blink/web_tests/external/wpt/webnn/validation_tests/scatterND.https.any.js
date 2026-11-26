// META: title=validation tests for WebNN API scatterND operation
// META: global=window
// META: variant=?cpu
// META: variant=?gpu
// META: variant=?npu
// META: script=../resources/utils_validation.js

'use strict';

const tests = [
  {
    name: '[scatterND] Test scatterND with valid tensors',
    input: {dataType: 'float32', shape: [4, 4, 4]},
    indices: {dataType: 'int32', shape: [2, 1]},
    updates: {dataType: 'float32', shape: [2, 4, 4]},
    output: {dataType: 'float32', shape: [4, 4, 4]}
  },
  {
    name:
        '[scatterND] Throw if updates tensor data type is not the same as input data type',
    input: {dataType: 'float32', shape: [4, 4, 4]},
    indices: {dataType: 'int32', shape: [2, 1]},
    updates: {dataType: 'float16', shape: [2, 4, 4]},
  },
  {
    name: '[scatterND] Throw if input is a scalar',
    input: {dataType: 'float32', shape: []},
    indices: {dataType: 'int32', shape: [2, 1]},
    updates: {dataType: 'float32', shape: [2, 4, 4]},
  },
  {
    name: '[scatterND] Throw if indices is a scalar',
    input: {dataType: 'float32', shape: [4, 4, 4]},
    indices: {dataType: 'int32', shape: []},
    updates: {dataType: 'float32', shape: [2, 4, 4]},
  },
  {
    name:
        '[scatterND] Throw if the size of last dimension of indices tensor is greater than input rank',
    input: {dataType: 'float32', shape: [4, 4, 4]},
    indices: {dataType: 'int32', shape: [2, 4]},
    updates: {dataType: 'float32', shape: [2, 4, 4]},
  },
  {
    name: '[scatterND] Throw if updates tensor shape is invalid.',
    input: {dataType: 'float32', shape: [4, 4, 4]},
    indices: {dataType: 'int32', shape: [2, 1]},
    // Updates tensor shape should be [2, 4, 4].
    updates: {dataType: 'float32', shape: [2, 3, 4]},
  }
];

tests.forEach(test => promise_test(async t => {
                const builder = new MLGraphBuilder(context);
                const input = builder.input('input', test.input);
                const indices = builder.input('indices', test.indices);
                const updates = builder.input('updates', test.updates);

                if (test.output) {
                  const output = builder.scatterND(input, indices, updates);
                  assert_equals(output.dataType, test.output.dataType);
                  assert_array_equals(output.shape, test.output.shape);
                } else {
                  const label = 'a_scatter_nd'
                  const options = {label};
                  const regexp = new RegExp('\\[' + label + '\\]');
                  assert_throws_with_label(
                      () => builder.scatterND(input, indices, updates, options),
                      regexp);
                }
              }, test.name));

multi_builder_test(async (t, builder, otherBuilder) => {
  const input = otherBuilder.input('input', {dataType: 'float32', shape: [8]});
  const indices = builder.input('indices', {dataType: 'int32', shape: [4, 1]});
  const updates = builder.input('indices', {dataType: 'int32', shape: [4]});

  assert_throws_js(TypeError, () => builder.scatterND(input, indices, updates));
}, '[scatterND] Throw if input is from another builder');

multi_builder_test(async (t, builder, otherBuilder) => {
  const input = builder.input('input', {dataType: 'float32', shape: [8]});
  const indices =
      otherBuilder.input('indices', {dataType: 'int32', shape: [4, 1]});
  const updates = builder.input('indices', {dataType: 'int32', shape: [4]});

  assert_throws_js(TypeError, () => builder.scatterND(input, indices, updates));
}, '[scatterND] Throw if indcies is from another builder');

multi_builder_test(async (t, builder, otherBuilder) => {
  const input = builder.input('input', {dataType: 'float32', shape: [8]});
  const indices = builder.input('indices', {dataType: 'int32', shape: [4, 1]});
  const updates =
      otherBuilder.input('indices', {dataType: 'int32', shape: [4]});

  assert_throws_js(TypeError, () => builder.scatterND(input, indices, updates));
}, '[scatterND] Throw if updates is from another builder');

validateOperandRankOutOfRange('scatterND', 'input', (builder, input) => {
  const indices = builder.input(
      'indices', {dataType: 'int32', shape: [kExampleDimSize, 1]});
  const updates = builder.input(
      'updates', {dataType: input.dataType, shape: [kExampleDimSize]});
  return builder.scatterND(input, indices, updates);
});

// Cartesian iteration over (input rank, indices rank) pairs allowed by
// opSupportLimits. scatterND ties updates rank to input + indices via
//   indices.shape[-1]         <= input.rank
//   updates.rank              = indices.rank - 1 + input.rank -
//   indices.shape[-1]
// so with indices.shape[-1] = 1 we get
//   updates.rank              = indices.rank + input.rank - 2
//   updates.shape             = indices.shape[:-1] + input.shape[1:]
// Skip pairs whose derived updates.rank falls outside updates.rankRange.
promise_test(async t => {
  const opLimits = context.opSupportLimits().scatterND;
  if (!opLimits)
    return;
  const {min: inputMin, max: inputMax} = opLimits.input.rankRange;
  const {min: indicesMin, max: indicesMax} = opLimits.indices.rankRange;
  const {min: updatesMin, max: updatesMax} = opLimits.updates.rankRange;
  const inputDataTypeInfo = pickSupportedDataTypeInfo(opLimits.input);
  const indicesDataTypeInfo = pickSupportedDataTypeInfo(opLimits.indices);
  if (!inputDataTypeInfo || !indicesDataTypeInfo)
    return;
  for (let inputRank = Math.max(inputMin, 1); inputRank <= inputMax;
       inputRank++) {
    for (let indicesRank = Math.max(indicesMin, 1); indicesRank <= indicesMax;
         indicesRank++) {
      const updatesRank = indicesRank + inputRank - 2;
      // Skip pairs whose derived updates rank falls outside the
      // updates rankRange.
      if (updatesRank < updatesMin || updatesRank > updatesMax)
        continue;
      const builder = new MLGraphBuilder(context);
      const inputShape = Array(inputRank).fill(kExampleDimSize);
      const indicesShape =
          Array(indicesRank - 1).fill(kExampleDimSize).concat([1]);
      const updatesShape = Array(updatesRank).fill(kExampleDimSize);
      const input =
          buildInputOperand(builder, 'input', inputDataTypeInfo, inputShape);
      const indices = buildInputOperand(builder, 'indices', indicesDataTypeInfo,
                                        indicesShape);
      const updates = buildInputOperand(builder, 'updates', inputDataTypeInfo,
                                        updatesShape);
      const output = builder.scatterND(input, indices, updates);
      assert_true(
          output instanceof MLOperand,
          `(input rank ${inputRank}, indices rank ${
              indicesRank}, updates rank ${updatesRank}) should be accepted`);
    }
  }
}, '[scatterND] accept every (input rank, indices rank) pair allowed by opSupportLimits');
