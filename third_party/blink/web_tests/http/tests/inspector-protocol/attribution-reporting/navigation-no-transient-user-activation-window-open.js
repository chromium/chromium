// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Test that using window.open with attributionsrc without transient user activation triggers an issue.');

  await dp.Audits.enable();

  const issue = dp.Audits.onceIssueAdded();

  // The `.name` is irrelevant for the test, but `evaluate` serializes the
  // result of the expression as JSON, which is not possible for the WindowProxy
  // returned by `open` itself.
  await session.evaluate(
      `window.open('https://a.com', '_blank', 'attributionsrc').name`);

  testRunner.log((await issue).params.issue, 'Issue reported: ');
  testRunner.completeTest();
})
