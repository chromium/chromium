// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test for https://issues.chromium.org/339141101 in which attempting
// to enable tracking when it was already enabled caused a DCHECK crash.
(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Test that Storage.setAttributionReportingTracking is idempotent and does not crash when called redundantly.');

  await dp.Storage.setAttributionReportingLocalTestingMode({enabled: true});

  await dp.Storage.setAttributionReportingTracking({enable: true});
  await dp.Storage.setAttributionReportingTracking({enable: true});

  dp.Runtime.evaluate({
    expression: `
    document.body.innerHTML = '<img attributionsrc="https://devtools.test:8443/inspector-protocol/attribution-reporting/resources/register-source-filter-data-and-agg-keys.php">';
  `
  });

  await dp.Storage.onceAttributionReportingSourceRegistered();

  await dp.Storage.setAttributionReportingTracking({enable: false});
  await dp.Storage.setAttributionReportingTracking({enable: false});

  testRunner.completeTest();
})
