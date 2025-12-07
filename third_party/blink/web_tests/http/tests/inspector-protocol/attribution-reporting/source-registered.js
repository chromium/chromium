// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Test that the Storage.attributionReportingSourceRegistered event is fired.');

  await dp.Storage.setAttributionReportingLocalTestingMode({enabled: true});
  await dp.Storage.setAttributionReportingTracking({enable: true});

  session.evaluate(`
    document.body.innerHTML = '<img attributionsrc="https://devtools.test:8443/inspector-protocol/attribution-reporting/resources/register-source-filter-data-and-agg-keys.php">'
  `);

  let {params} = await dp.Storage.onceAttributionReportingSourceRegistered();
  testRunner.log(params, '', ['sourceOrigin', 'time']);
  ({params} =
       await dp.Storage.onceAttributionReportingVerboseDebugReportSent());
  testRunner.log(params, '', ['source_site', 'url']);
  testRunner.completeTest();
})
