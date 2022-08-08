// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Test that an attributionsrc request that is only eligible for triggers triggers an issue when it tries to register a source.`);

  await dp.Audits.enable();
  await page.navigate(
      'https://devtools.test:8443/inspector-protocol/attribution-reporting/resources/impression.html');
  await page.loadHTML(`<body>`);

  const issuePromise = dp.Audits.onceIssueAdded();
  await dp.Runtime.evaluate({
    expression:
        `fetch('/inspector-protocol/attribution-reporting/resources/register-source-and-trigger.php',{headers:{'Attribution-Reporting-Eligible':'trigger'}})`,
  });
  const issue = await issuePromise;
  testRunner.log(issue.params.issue, 'Issue reported: ', ['request']);
  testRunner.completeTest();
})
