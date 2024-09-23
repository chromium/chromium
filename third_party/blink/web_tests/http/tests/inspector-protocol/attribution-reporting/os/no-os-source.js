// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Test that an attributionsrc request that is eligible for source triggers an issue when OS is preferred but there is no OS source registration.');

  await dp.Audits.enable();

  const issue = dp.Audits.onceIssueAdded();

  await dp.Runtime.evaluate({expression: `
    fetch('/inspector-protocol/attribution-reporting/resources/register-web-source-prefer-os.php',
        {keepalive: true,
         attributionReporting: {
          eventSourceEligible: true,
          triggerEligible: false,
        }});
  `});

  testRunner.log((await issue).params.issue, 'Issue reported: ', ['request']);
  testRunner.completeTest();
})
