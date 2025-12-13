// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startHTML(
      '<a href="https://devtools.test:8443/inspector-protocol/attribution-reporting/resources/register-invalid-source.php" attributionsrc target="_blank">Link</a>',
      'Test that clicking an attributionsrc anchor which returns an invalid Attribution-Reporting-Register-Source header triggers an issue.');

  await dp.Audits.enable();

  session.evaluateAsyncWithUserGesture(`document.querySelector('a').click()`);

  let issue;
  do {
    issue = await dp.Audits.onceIssueAdded();
  } while (issue.params.issue.code !== 'AttributionReportingIssue');

  testRunner.log(issue.params.issue, 'Issue reported: ', ['request']);

  testRunner.completeTest();
});
