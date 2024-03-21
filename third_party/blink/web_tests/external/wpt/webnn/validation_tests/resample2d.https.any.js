// META: title=validation tests for WebNN API resample2d operation
// META: global=window,dedicatedworker
// META: script=../resources/utils_validation.js

'use strict';

validateOptionsAxes('resample2d', 4);

validateInputFromAnotherBuilder(
    'resample2d', {dataType: 'float32', dimensions: [2, 2, 2, 2]});
