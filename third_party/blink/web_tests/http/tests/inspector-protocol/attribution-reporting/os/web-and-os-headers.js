// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Test that an attributionsrc request triggers an issue when it tries to register a web trigger and an OS trigger together.');

  await dp.Audits.enable();

  const issue = dp.Audits.onceIssueAdded();

  await dp.Runtime.evaluate({expression: `
    fetch('/inspector-protocol/attribution-reporting/resources/register-web-and-os-trigger.php',
        {keepalive: true});
  `});

  testRunner.log((await issue).params.issue, 'Issue reported: ', ['request']);
  testRunner.completeTest();
})
