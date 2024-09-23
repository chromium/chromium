// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Test that an attributionsrc request triggers an issue for an invalid info header.');

  await dp.Audits.enable();

  const issue = dp.Audits.onceIssueAdded();

  await dp.Runtime.evaluate({expression: `
    fetch('/inspector-protocol/attribution-reporting/resources/register-with-invalid-info-header.php',
        {keepalive: true,
         attributionReporting: {
          eventSourceEligible: true,
          triggerEligible: false,
        }});
  `});

  testRunner.log((await issue).params.issue, 'Issue reported: ', ['request']);
  testRunner.completeTest();
})
