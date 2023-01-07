// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Test that an attributionsrc request that is eligible for sources and triggers triggers an issue when it tries to register a source and trigger together.');

  await dp.Audits.enable();

  const issue = dp.Audits.onceIssueAdded();

  await dp.Runtime.evaluate({expression: `
    fetch('/inspector-protocol/attribution-reporting/resources/register-source-and-trigger.php',{headers:{'Attribution-Reporting-Eligible':'event-source,trigger'}});
  `});

  testRunner.log((await issue).params.issue, 'Issue reported: ', ['request']);
  testRunner.completeTest();
})
