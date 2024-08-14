// META: title=validation tests for WebNN API sigmoid operation
// META: global=window,dedicatedworker
// META: script=../resources/utils_validation.js

'use strict';

validateInputFromAnotherBuilder('sigmoid');

const label = 'sigmoid_xxx';
const regrexp = new RegExp('\\[' + label + '\\]');
validateSingleInputOperation('sigmoid', label, regrexp);
