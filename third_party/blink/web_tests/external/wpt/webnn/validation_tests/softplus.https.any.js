// META: title=validation tests for WebNN API softplus operation
// META: global=window,dedicatedworker
// META: script=../resources/utils_validation.js

'use strict';

validateInputFromAnotherBuilder('softplus');

const label = 'softplus_xxx';
const regrexp = new RegExp('\\[' + label + '\\]');
validateSingleInputOperation('softplus', label, regrexp);
