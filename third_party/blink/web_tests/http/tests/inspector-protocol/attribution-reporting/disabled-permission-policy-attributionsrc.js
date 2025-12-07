// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startURL(
      'https://devtools.test:8443/inspector-protocol/attribution-reporting/resources/permissions-policy-no-conversion-measurement.php',
      'Test that clicking an anchor with attributionsrc triggers an issue when the attribution-reporting Permissions Policy is disabled.');

  await dp.Audits.enable();

  await session.evaluate(`
    document.body.innerHTML = '<a id="adlink" href="/" attributionsrc="/" target="_blank">Link</a>'
  `);

  session.evaluateAsyncWithUserGesture(
      `document.getElementById('adlink').click()`);

  let issue;
  do {
    issue = await dp.Audits.onceIssueAdded();
  } while (issue.params.issue.code !== 'AttributionReportingIssue');

  testRunner.log(issue.params.issue, 'Issue reported: ', ['violatingNodeId']);
  testRunner.completeTest();
})
