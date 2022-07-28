// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Test that registering a trigger using a subbresource request to an insecure reporting origin triggers an issue.`);

  await dp.Audits.enable();

  await page.loadHTML(
      `<img src="http://devtools.test:8000/inspector-protocol/attribution-reporting/resources/register-trigger.php">`);

  const issuePromise = dp.Audits.onceIssueAdded();
  const issue = await issuePromise;
  testRunner.log(issue.params.issue, 'Issue reported: ', ['request']);
  testRunner.completeTest();
})
