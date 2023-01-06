// META: title=test WebNN API concat operation
// META: global=window,dedicatedworker
// META: script=./resources/utils.js
// META: timeout=long

'use strict';

// https://webmachinelearning.github.io/webnn/#api-mlgraphbuilder-concat

const buildConcat = (builder, resources) => {
  // MLOperand concat(sequence<MLOperand> inputs, long axis);
  const namedOutputOperand = {};
  const inputOperands = [];
  for (let input of resources.inputs) {
    inputOperands.push(builder.input(input.name, {type: input.type, dimensions: input.shape}));
  }
  namedOutputOperand[resources.expected.name] = builder.concat(inputOperands, resources.axis);
  return namedOutputOperand;
};

testWebNNOperation('concat', '/webnn/resources/test_data/concat.json', buildConcat);