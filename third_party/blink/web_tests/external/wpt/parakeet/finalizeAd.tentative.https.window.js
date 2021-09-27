// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
'use strict';

test(() => {
  assert_not_equals(navigator.finalizeAd, undefined);
}, "finalizeAd() should be supported on the navigator interface.");

promise_test(async t => {
  const createPromise = navigator.finalizeAd();

  await promise_rejects_dom(t, "NotSupportedError", createPromise);
}, "finalizeAd() should reject with NotSupported initially.");
