// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Test that a site with disabled conversion measurement Permissions Policy reports an issue when an attribution link is clicked`);

  await dp.Audits.enable();
  await page.navigate('https://devtools.test:8443/inspector-protocol/conversion/resources/permissions-policy-no-conversion-measurement.php');

  await page.loadHTML(`
  <a id="adlink" href="https://a.com"
  attributiondestination="https://does-not-matter.com"
  attributionsourceeventid="12" target="_blank">Impression (ad)
  </a>`);

  const issuePromise = dp.Audits.onceIssueAdded();
  await dp.Runtime.evaluate({expression: `document.getElementById('adlink').click()`, userGesture: true});
  const issue = await issuePromise;
  testRunner.log(issue.params.issue, "Issue reported: ", ['frame', 'violatingNodeId']);
  testRunner.completeTest();
})
