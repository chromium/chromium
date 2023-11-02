// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Test that initiating too many concurrent attributionsrc requests triggers an issue.');

  await dp.Audits.enable();

  const issue = dp.Audits.onceIssueAdded();

  const maxConcurrentRequests = 30;
  for (let i = 0; i < maxConcurrentRequests + 1; i++) {
    await dp.Runtime.evaluate({expression: `
      document.createElement('img').attributionSrc='/inspector-protocol/attribution-reporting/resources/sleep.php';
    `});
  }

  testRunner.log((await issue).params.issue, 'Issue reported: ', ['violatingNodeId']);
  testRunner.completeTest();
})
