// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Test that an attributionsrc request whose Attribution-Reporting-Eligible header contains navigation-source triggers an issue.');

  await dp.Audits.enable();

  const issue = dp.Audits.onceIssueAdded();

  await dp.Runtime.evaluate({expression: `
    fetch('/inspector-protocol/attribution-reporting/resources/register-trigger.php',{headers:{'Attribution-Reporting-Eligible':'navigation-source'}});
  `});

  testRunner.log((await issue).params.issue, 'Issue reported: ', ['request']);
  testRunner.completeTest();
})
