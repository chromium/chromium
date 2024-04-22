// META: title=validation tests for WebNN API softplus operation
// META: global=window,dedicatedworker
// META: script=../resources/utils_validation.js

'use strict';

validateInputFromAnotherBuilder('softplus');

validateUnaryOperation(
    'softplus', floatingPointTypes, /*alsoBuildActivation=*/ true);

promise_test(async t => {
  const options = {steepness: 1.3};
  const input =
      builder.input('input', {dataType: 'float32', dimensions: [1, 2, 3]});
  const output = builder.softplus(input, options);
  assert_equals(output.dataType(), 'float32');
  assert_array_equals(output.shape(), [1, 2, 3]);
}, '[softplus] Test building an operator with options');

promise_test(async t => {
  const options = {steepness: 1.2};
  builder.softplus(options);
}, '[softplus] Test building an activation with options.steepness == 1.2');

promise_test(async t => {
  const options = {steepness: 0};
  builder.softplus(options);
}, '[softplus] Test building an activation with options.steepness == 0');

promise_test(async t => {
  const options = {steepness: Infinity};
  const input = builder.input('input', {dataType: 'float16', dimensions: []});
  assert_throws_js(TypeError, () => builder.softplus(input, options));
}, '[softplus] Throw if options.steepness is Infinity when building an operator');

promise_test(async t => {
  const options = {steepness: NaN};
  assert_throws_js(TypeError, () => builder.softplus(options));
}, '[softplus] Throw if options.steepness is NaN when building an activation');
