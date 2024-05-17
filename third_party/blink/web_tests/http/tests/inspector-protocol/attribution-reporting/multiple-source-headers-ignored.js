// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { dp } = await testRunner.startHTML(
    '<a href="https://devtools.test:8443/inspector-protocol/attribution-reporting/resources/register-multiple-sources.php" attributionsrc target="_blank">Link</a>',
    "Test that clicking an attributionsrc anchor which returns multiple Attribution-Reporting-Register-Source headers triggers an issue."
  );

  await dp.Audits.enable();

  const issue = dp.Audits.onceIssueAdded();

  await dp.Runtime.evaluate({
    expression: `document.querySelector('a').click()`,
    userGesture: true,
  });

  testRunner.log((await issue).params.issue, "Issue reported: ", [
    "request",
  ]);

  testRunner.completeTest();
});
