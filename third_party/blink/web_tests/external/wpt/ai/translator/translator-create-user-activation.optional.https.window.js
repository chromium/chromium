// META: title=Translator Create User Activation
// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=../resources/util.js
// META: timeout=long

'use strict';

// Mocked model download state may be shared between test cases in the same file
// (see e.g. `EchoAIManagerImpl`), so this test case is kept in a separate file.
// TODO(crbug.com/390246212): Support model state controls for WPTs.
promise_test(async t => {
  // Create requires user activation when availability is 'downloadable'.
  const languagePair = {sourceLanguage: 'en', targetLanguage: 'ja'};
  assert_implements_optional(await Translator.availability(languagePair) == 'downloadable');
  assert_false(navigator.userActivation.isActive);
  await promise_rejects_dom(t, 'NotAllowedError', Translator.create(languagePair));
  await test_driver.bless();
  const createPromise = Translator.create(languagePair);
  // User activation is not consumed by the create call.
  assert_true(navigator.userActivation.isActive);
  consumeTransientUserActivation();
  await createPromise;

  // Create does not require transient user activation.
  assert_equals(await Translator.availability(languagePair), 'available');
  assert_false(navigator.userActivation.isActive);
  await Translator.create(languagePair);
}, 'Create requires sticky user activation when availability is "downloadable"');
