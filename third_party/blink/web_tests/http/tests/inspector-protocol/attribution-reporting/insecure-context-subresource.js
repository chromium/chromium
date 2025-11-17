// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startURL(
      'http://devtools.test:8000/inspector-protocol/resources/empty.html',
      'Test that registering a trigger using a subresource request in an insecure context triggers an issue.');

  await dp.Audits.enable();

  session.evaluate(`
    document.body.innerHTML = '<img src="https://devtools.test:8443/inspector-protocol/attribution-reporting/resources/register-trigger.php">'
  `);

  let issue;
  do {
    issue = await dp.Audits.onceIssueAdded();
  } while (issue.params.issue.code !== 'AttributionReportingIssue');

  testRunner.log(issue.params.issue, 'Issue reported: ', ['request']);
  testRunner.completeTest();
})
