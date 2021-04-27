// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Test that clicking an attribution link in insecure contexts triggers an issue.`);

  await dp.Audits.enable();
  await page.navigate('http://devtools.test:8000/inspector-protocol/conversion/resources/impression.html');

  await dp.Runtime.evaluate({expression: `
    createImpressionAnchor('adlink', 'https://a.com', 42, 'https://does-not-matter.com', '_blank');`});

  const issuePromise = dp.Audits.onceIssueAdded();
  await dp.Runtime.evaluate({expression: `document.getElementById('adlink').click()`, userGesture: true});
  const issue = await issuePromise;
  testRunner.log(issue.params.issue, "Issue reported: ", ['frame', 'violatingNodeId']);
  testRunner.completeTest();
})
