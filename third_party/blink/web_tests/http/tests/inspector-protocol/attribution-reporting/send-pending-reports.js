// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Test that Storage.sendPendingAttributionReports succeeds.');

  await dp.Storage.setAttributionReportingTracking({enable: true});

  // Note: we deliberately do *not* enable local-testing mode, because that mode
  // causes reports to be sent immediately, which would create a race condition
  // between the inherent send and the call to `sendPendingAttributionReports()`
  // below. As a result, however, noise is *also* enabled, which means that the
  // below trigger can fail to result in a report.
  //
  // Therefore, to prevent flakiness in which 0 reports would be sent instead of
  // 1, we short-circuit the test when we detect that the source has been
  // noised. This should only happen a tiny fraction of the time.
  //
  // TODO(crbug.com/40273482): Remove this short-circuiting once noise and
  // report delays are independently configurable.

  session.evaluate(`
    fetch('/inspector-protocol/attribution-reporting/resources/register-source-localhost.php',
          {attributionReporting: {eventSourceEligible: true, triggerEligible: false}});
  `);

  await dp.Storage.onceAttributionReportingSourceRegistered();

  session.evaluate(`
    fetch('/inspector-protocol/attribution-reporting/resources/register-event-trigger.php');
  `);

  const {params} = await dp.Storage.onceAttributionReportingTriggerRegistered();
  if (params.eventLevelResult === 'neverAttributedSource' ||
      params.eventLevelResult === 'falselyAttributedSource') {
    testRunner.log({numSent: 1});
    testRunner.completeTest();
    return;
  }

  const {result} = await dp.Storage.sendPendingAttributionReports();
  testRunner.log(result);
  testRunner.completeTest();
})
