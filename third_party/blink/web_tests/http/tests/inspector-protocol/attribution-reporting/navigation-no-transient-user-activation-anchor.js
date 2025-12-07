// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Test that clicking an attributionsrc anchor without transient user activation triggers an issue.');

  await dp.Audits.enable();

  await session.evaluate(`
    document.body.innerHTML = '<a href="https://devtools.test:8443/" attributionsrc target="_blank">Link</a>'
  `);

  session.evaluate(`document.querySelector('a').click()`);

  let issue;
  do {
    issue = await dp.Audits.onceIssueAdded();
  } while (issue.params.issue.code !== 'AttributionReportingIssue');

  testRunner.log(issue.params.issue, 'Issue reported: ', ['violatingNodeId']);
  testRunner.completeTest();
})
