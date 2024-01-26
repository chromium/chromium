// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Test that using window.open with attributionsrc without transient user activation triggers an issue.');

  await dp.Audits.enable();

  const issue = dp.Audits.onceIssueAdded();

  await dp.Runtime.evaluate({
    expression: `window.open('https://a.com', '_blank', 'attributionsrc');`,
    userGesture: false,
  });

  testRunner.log((await issue).params.issue, 'Issue reported: ');
  testRunner.completeTest();
})
