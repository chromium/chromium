// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Test that the Storage.attributionReportingTriggerRegistered event is fired.');

  await dp.Storage.setAttributionReportingLocalTestingMode({enabled: true});
  await dp.Storage.setAttributionReportingTracking({enable: true});

  session.evaluate(`
    fetch('/inspector-protocol/attribution-reporting/resources/register-complete-trigger.php');
  `);

  const {params} = await dp.Storage.onceAttributionReportingTriggerRegistered();
  testRunner.log(params, '');
  testRunner.completeTest();
})
