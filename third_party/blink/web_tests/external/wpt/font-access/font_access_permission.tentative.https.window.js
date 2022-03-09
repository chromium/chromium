//META: script=/resources/testdriver.js
//META: script=/resources/testdriver-vendor.js

'use strict';

promise_test(async t => {
  await promise_rejects_dom(t, 'NotAllowedError', navigator.fonts.query());
}, 'query(): permission not given');

promise_test(async t => {
  await test_driver.set_permission({name: 'font-access'}, 'denied');
  await promise_rejects_dom(t, 'NotAllowedError', navigator.fonts.query());
}, 'query(): permission denied');

promise_test(async t => {
  await test_driver.set_permission({name: 'font-access'}, 'granted');
  const fonts = await navigator.fonts.query();
  assert_greater_than_equal(
      fonts.length, 1, 'Fonts are returned with permission granted.');
}, 'query(): permission granted');
