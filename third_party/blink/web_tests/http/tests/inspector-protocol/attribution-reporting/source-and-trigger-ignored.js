// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Test that an attributionsrc request that is not eligible for sources or triggers triggers issues when it tries to register them.');

  await dp.Audits.enable();

  const issue1 = dp.Audits.onceIssueAdded();
  const issue2 = dp.Audits.onceIssueAdded();

  await dp.Runtime.evaluate({
    expression: `
    fetch('/inspector-protocol/attribution-reporting/resources/register-source-and-trigger.php',
        {attributionReporting: {
          eventSourceEligible: false,
          triggerEligible: false,
        }});
  `
  });

  testRunner.log((await issue1).params.issue, 'Issue 1 reported: ', ['request']);
  testRunner.log((await issue2).params.issue, 'Issue 2 reported: ', ['request']);
  testRunner.completeTest();
})
