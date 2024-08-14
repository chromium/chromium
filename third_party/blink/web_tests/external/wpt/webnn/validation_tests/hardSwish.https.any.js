// META: title=validation tests for WebNN API hardSwish operation
// META: global=window,dedicatedworker
// META: script=../resources/utils_validation.js

'use strict';

validateInputFromAnotherBuilder('hardSwish');

const label = 'hard_swish';
const regrexp = new RegExp('\\[' + label + '\\]');
validateUnaryOperation('hardSwish', floatingPointTypes, label, regrexp);
