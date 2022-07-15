// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Test that clicking an anchor with valueless attributionsrc triggers an issue when the attribution-reporting Permissions Policy is disabled.`);

  await dp.Audits.enable();
  await page.navigate(
      'https://devtools.test:8443/inspector-protocol/attribution-reporting/resources/permissions-policy-no-conversion-measurement.php');

  await page.loadHTML(`
  <a id="adlink" href="https://a.com"
  attributionsrc target="_blank">Impression</a>`);

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
