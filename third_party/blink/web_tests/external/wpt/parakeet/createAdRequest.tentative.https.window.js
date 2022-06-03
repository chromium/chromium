// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
'use strict';

test(() => {
  assert_not_equals(navigator.createAdRequest, undefined);
}, "createAdRequest() should be supported on the navigator interface.");

promise_test(async t => {
  const createPromise = navigator.createAdRequest();

  await promise_rejects_dom(t, "NotSupportedError", createPromise);
}, "createAdRequest() should reject with NotSupported initially.");
