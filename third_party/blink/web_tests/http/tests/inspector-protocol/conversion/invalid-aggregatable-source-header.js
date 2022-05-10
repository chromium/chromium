// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Test that an attributionsrc response with an invalid Attribution-Reporting-Register-Aggregatable-Source header triggers an issue.`);

  await dp.Audits.enable();
  await page.navigate(
      'https://devtools.test:8443/inspector-protocol/conversion/resources/impression.html');

  await page.loadHTML(
      `<img attributionsrc="https://devtools.test:8443/inspector-protocol/conversion/resources/register-invalid-aggregatable-source.php">`);

  const issuePromise = dp.Audits.onceIssueAdded();
  const issue = await issuePromise;
  testRunner.log(
      issue.params.issue, 'Issue reported: ', ['frame', 'request']);
  testRunner.completeTest();
})
