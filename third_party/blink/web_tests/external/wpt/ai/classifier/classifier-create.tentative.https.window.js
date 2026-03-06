// META: title=Classifier Create
// META: script=/resources/testdriver.js
// META: script=../resources/util.js
// META: timeout=long

'use strict';

promise_test(async (t) => {
  const availability = await Classifier.availability();
  if (availability === 'unavailable') {
    // TODO(crbug.com/487291285): Update NotAllowedError to NotSupportedError.
    await promise_rejects_dom(
      t,
      'NotAllowedError',
      Classifier.create()
    );
  } else {
    // If the API is available, it should successfully create.
    const classifier = await Classifier.create();
    assert_true(!!classifier, 'Classifier was successfully created');
  }
}, 'Classifier.create() behavior depends on availability');
