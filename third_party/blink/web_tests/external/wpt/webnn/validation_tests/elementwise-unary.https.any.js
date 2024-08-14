// META: title=validation tests for WebNN API element-wise unary operations
// META: global=window,dedicatedworker
// META: script=../resources/utils_validation.js

'use strict';

const kElementwiseUnaryOperators = [
  'abs', 'ceil', 'cos', 'erf', 'exp', 'floor', 'identity', 'log', 'neg',
  'reciprocal', 'sin', 'sqrt', 'tan'
];

kElementwiseUnaryOperators.forEach((operatorName) => {
  validateInputFromAnotherBuilder(operatorName);
});

const label = 'elementwise_unary_op';
const regrexp = new RegExp('\\[' + label + '\\]');
kElementwiseUnaryOperators.forEach((operatorName) => {
  validateSingleInputOperation(operatorName, label, regrexp);
});
