// META: title=validation tests for WebNN API tanh operation
// META: global=window,dedicatedworker
// META: script=../resources/utils_validation.js

'use strict';

validateInputFromAnotherBuilder('tanh');

const label = 'tanh-xxx';
const regrexp = new RegExp('\\[' + label + '\\]');
validateUnaryOperation('tanh', floatingPointTypes, label, regrexp);
