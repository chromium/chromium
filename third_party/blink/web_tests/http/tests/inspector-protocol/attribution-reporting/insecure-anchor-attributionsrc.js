// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Test that clicking an anchor with an insecure attributionsrc triggers an issue.`);

  await dp.Audits.enable();

  await page.loadHTML(`
  <a id="adlink" href="https://a.com"
  attributionsrc="http://insecure.com"
  target="_blank">Impression</a>`);

  const issuePromise = dp.Audits.onceIssueAdded();
  await dp.Runtime.evaluate({
    expression: `document.getElementById('adlink').click()`,
    userGesture: true
  });
  const issue = await issuePromise;
  testRunner.log(
      issue.params.issue, 'Issue reported: ', ['frame', 'violatingNodeId']);
  testRunner.completeTest();
})
