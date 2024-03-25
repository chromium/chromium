// META: title=validation tests for WebNN API lstmCell operation
// META: global=window,dedicatedworker
// META: script=../resources/utils_validation.js

'use strict';

const batchSize = 3, inputSize = 4, hiddenSize = 5;

// Dimensions required of required inputs.
const kValidInputDimensions = [batchSize, inputSize];
const kValidWeightDimensions = [4 * hiddenSize, inputSize];
const kValidRecurrentWeightDimensions = [4 * hiddenSize, hiddenSize];
const kValidHiddenStateDimensions = [batchSize, hiddenSize];
const kValidCellStateDimensions = [batchSize, hiddenSize];
// Dimensions required of optional inputs.
const kValidBiasDimensions = [4 * hiddenSize];
const kValidPeepholeWeightDimensions = [3 * hiddenSize];

// Example descriptors which are valid according to the above dimensions.
const kExampleInputDescriptor = {
  dataType: 'float32',
  dimensions: kValidInputDimensions
};
const kExampleWeightDescriptor = {
  dataType: 'float32',
  dimensions: kValidWeightDimensions
};
const kExampleRecurrentWeightDescriptor = {
  dataType: 'float32',
  dimensions: kValidRecurrentWeightDimensions
};
const kExampleHiddenStateDescriptor = {
  dataType: 'float32',
  dimensions: kValidHiddenStateDimensions
};
const kExampleCellStateDescriptor = {
  dataType: 'float32',
  dimensions: kValidCellStateDimensions
};
const kExampleBiasDescriptor = {
  dataType: 'float32',
  dimensions: kValidBiasDimensions
};
const kExamplePeepholeWeightDescriptor = {
  dataType: 'float32',
  dimensions: kValidPeepholeWeightDimensions
};

multi_builder_test(async (t, builder, otherBuilder) => {
  const inputFromOtherBuilder =
      otherBuilder.input('input', kExampleInputDescriptor);

  const weight = builder.input('weight', kExampleWeightDescriptor);
  const recurrentWeight =
      builder.input('recurrentWeight', kExampleRecurrentWeightDescriptor);
  const hiddenState =
      builder.input('hiddenState', kExampleHiddenStateDescriptor);
  const cellState = builder.input('cellState', kExampleCellStateDescriptor);
  assert_throws_js(
      TypeError,
      () => builder.lstmCell(
          inputFromOtherBuilder, weight, recurrentWeight, hiddenState,
          cellState, hiddenSize));
}, '[lstmCell] throw if input is from another builder');

multi_builder_test(async (t, builder, otherBuilder) => {
  const weightFromOtherBuilder =
      otherBuilder.input('weight', kExampleWeightDescriptor);

  const input = builder.input('input', kExampleInputDescriptor);
  const recurrentWeight =
      builder.input('recurrentWeight', kExampleRecurrentWeightDescriptor);
  const hiddenState =
      builder.input('hiddenState', kExampleHiddenStateDescriptor);
  const cellState = builder.input('cellState', kExampleCellStateDescriptor);
  assert_throws_js(
      TypeError,
      () => builder.lstmCell(
          input, weightFromOtherBuilder, recurrentWeight, hiddenState,
          cellState, hiddenSize));
}, '[lstmCell] throw if weight is from another builder');

multi_builder_test(async (t, builder, otherBuilder) => {
  const recurrentWeightFromOtherBuilder =
      otherBuilder.input('recurrentWeight', kExampleRecurrentWeightDescriptor);

  const input = builder.input('input', kExampleInputDescriptor);
  const weight = builder.input('weight', kExampleWeightDescriptor);
  const hiddenState =
      builder.input('hiddenState', kExampleHiddenStateDescriptor);
  const cellState = builder.input('cellState', kExampleCellStateDescriptor);
  assert_throws_js(
      TypeError,
      () => builder.lstmCell(
          input, weight, recurrentWeightFromOtherBuilder, hiddenState,
          cellState, hiddenSize));
}, '[lstmCell] throw if recurrentWeight is from another builder');

multi_builder_test(async (t, builder, otherBuilder) => {
  const hiddenStateFromOtherBuilder =
      otherBuilder.input('hiddenState', kExampleHiddenStateDescriptor);

  const input = builder.input('input', kExampleInputDescriptor);
  const weight = builder.input('weight', kExampleWeightDescriptor);
  const recurrentWeight =
      builder.input('recurrentWeight', kExampleRecurrentWeightDescriptor);
  const cellState = builder.input('cellState', kExampleCellStateDescriptor);
  assert_throws_js(
      TypeError,
      () => builder.lstmCell(
          input, weight, recurrentWeight, hiddenStateFromOtherBuilder,
          cellState, hiddenSize));
}, '[lstmCell] throw if hiddenState is from another builder');

multi_builder_test(async (t, builder, otherBuilder) => {
  const cellStateFromOtherBuilder =
      otherBuilder.input('cellState', kExampleCellStateDescriptor);

  const input = builder.input('input', kExampleInputDescriptor);
  const weight = builder.input('weight', kExampleWeightDescriptor);
  const recurrentWeight =
      builder.input('recurrentWeight', kExampleRecurrentWeightDescriptor);
  const hiddenState =
      builder.input('hiddenState', kExampleHiddenStateDescriptor);
  assert_throws_js(
      TypeError,
      () => builder.lstmCell(
          input, weight, recurrentWeight, hiddenState,
          cellStateFromOtherBuilder, hiddenSize));
}, '[lstmCell] throw if cellState is from another builder');

multi_builder_test(async (t, builder, otherBuilder) => {
  const biasFromOtherBuilder =
      otherBuilder.input('bias', kExampleBiasDescriptor);
  const options = {bias: biasFromOtherBuilder};

  const input = builder.input('input', kExampleInputDescriptor);
  const weight = builder.input('weight', kExampleWeightDescriptor);
  const recurrentWeight =
      builder.input('recurrentWeight', kExampleRecurrentWeightDescriptor);
  const hiddenState =
      builder.input('hiddenState', kExampleHiddenStateDescriptor);
  const cellState = builder.input('cellState', kExampleCellStateDescriptor);
  assert_throws_js(
      TypeError,
      () => builder.lstmCell(
          input, weight, recurrentWeight, hiddenState, cellState, hiddenSize,
          options));
}, '[lstmCell] throw if bias option is from another builder');

multi_builder_test(async (t, builder, otherBuilder) => {
  const recurrentBiasFromOtherBuilder =
      otherBuilder.input('bias', kExampleBiasDescriptor);
  const options = {recurrentBias: recurrentBiasFromOtherBuilder};

  const input = builder.input('input', kExampleInputDescriptor);
  const weight = builder.input('weight', kExampleWeightDescriptor);
  const recurrentWeight =
      builder.input('recurrentWeight', kExampleRecurrentWeightDescriptor);
  const hiddenState =
      builder.input('hiddenState', kExampleHiddenStateDescriptor);
  const cellState = builder.input('cellState', kExampleCellStateDescriptor);
  assert_throws_js(
      TypeError,
      () => builder.lstmCell(
          input, weight, recurrentWeight, hiddenState, cellState, hiddenSize,
          options));
}, '[lstmCell] throw if recurrentBias option is from another builder');

multi_builder_test(async (t, builder, otherBuilder) => {
  const peepholeWeightFromOtherBuilder =
      otherBuilder.input('peepholeWeight', kExamplePeepholeWeightDescriptor);
  const options = {peepholeWeight: peepholeWeightFromOtherBuilder};

  const input = builder.input('input', kExampleInputDescriptor);
  const weight = builder.input('weight', kExampleWeightDescriptor);
  const recurrentWeight =
      builder.input('recurrentWeight', kExampleRecurrentWeightDescriptor);
  const hiddenState =
      builder.input('hiddenState', kExampleHiddenStateDescriptor);
  const cellState = builder.input('cellState', kExampleCellStateDescriptor);
  assert_throws_js(
      TypeError,
      () => builder.lstmCell(
          input, weight, recurrentWeight, hiddenState, cellState, hiddenSize,
          options));
}, '[lstmCell] throw if peepholeWeight option is from another builder');

multi_builder_test(async (t, builder, otherBuilder) => {
  const activation = builder.clamp();
  const activationFromOtherBuilder = otherBuilder.clamp();
  const options = {activations: [activation, activationFromOtherBuilder]};

  const input = builder.input('input', kExampleInputDescriptor);
  const weight = builder.input('weight', kExampleWeightDescriptor);
  const recurrentWeight =
      builder.input('recurrentWeight', kExampleRecurrentWeightDescriptor);
  const hiddenState =
      builder.input('hiddenState', kExampleHiddenStateDescriptor);
  const cellState = builder.input('cellState', kExampleCellStateDescriptor);
  assert_throws_js(
      TypeError,
      () => builder.lstmCell(
          input, weight, recurrentWeight, hiddenState, cellState, hiddenSize,
          options));
}, '[lstmCell] throw if activation option is from another builder');
