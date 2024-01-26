// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Test that clicking an attributionsrc anchor without transient user activation triggers an issue.');

  await dp.Audits.enable();

  await dp.Runtime.evaluate({
    expression: `
    document.body.innerHTML = '<a href="https://a.com" attributionsrc target="_blank">Link</a>';
  `
  });

  const issue = dp.Audits.onceIssueAdded();

  await dp.Runtime.evaluate({
    expression: `document.querySelector('a').click()`,
    userGesture: false,
  });

  testRunner.log(
      (await issue).params.issue, 'Issue reported: ', ['violatingNodeId']);
  testRunner.completeTest();
})
