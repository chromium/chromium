//META: script=/resources/testdriver.js
//META: script=/resources/testdriver-vendor.js

'use strict';

promise_test(async t => {
  const fonts = await navigator.fonts.query();
  assert_equals(
      fonts.length, 0, 'Fonts are not returned with permission not given.');
}, 'query(): permission not given');

promise_test(async t => {
  await test_driver.set_permission({name: 'local-fonts'}, 'denied');
  const fonts = await navigator.fonts.query();
  assert_equals(
      fonts.length, 0, 'Fonts are not returned with permission denied.');
}, 'query(): permission denied');

promise_test(async t => {
  await test_driver.set_permission({name: 'local-fonts'}, 'granted');
  const fonts = await navigator.fonts.query();
  assert_greater_than_equal(
      fonts.length, 1, 'Fonts are returned with permission granted.');
}, 'query(): permission granted');
