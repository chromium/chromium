// META: title=validation tests for WebNN API element-wise logical operations
// META: global=window,dedicatedworker
// META: script=../resources/utils_validation.js

'use strict';

const kElementwiseLogicalBinaryOperators = [
  'equal',
  'greater',
  'greaterOrEqual',
  'lesser',
  'lesserOrEqual',
];

const label = 'elementwise_logic_op';
const regrexp = new RegExp('\\[' + label + '\\]');

kElementwiseLogicalBinaryOperators.forEach((operatorName) => {
  validateTwoInputsOfSameDataType(operatorName, label, regrexp);
  validateTwoInputsFromMultipleBuilders(operatorName);
  validateTwoInputsBroadcastable(operatorName, label, regrexp);
});

// The `logicalNot()` operator is unary.
validateInputFromAnotherBuilder('logicalNot');
