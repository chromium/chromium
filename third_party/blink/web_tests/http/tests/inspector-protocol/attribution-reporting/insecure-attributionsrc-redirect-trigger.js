// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Test that an attributionsrc that redirects to an insecure origin and tries to register a trigger triggers an issue.');

  await dp.Audits.enable();

  session.evaluate(`
    document.body.innerHTML = '<img attributionsrc="https://devtools.test:8443/inspector-protocol/attribution-reporting/resources/redirect-to-insecure-origin-and-register-trigger.php">'
  `);

  let issue;
  do {
    issue = await dp.Audits.onceIssueAdded();
  } while (issue.params.issue.code !== 'AttributionReportingIssue');

  testRunner.log(issue.params.issue, 'Issue reported: ', ['request']);
  testRunner.completeTest();
})
