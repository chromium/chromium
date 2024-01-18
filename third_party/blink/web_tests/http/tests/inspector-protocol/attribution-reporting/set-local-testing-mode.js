// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Test that the Storage.setAttributionReportingLocalTestingMode command is handled.');

  testRunner.log(await dp.Storage.setAttributionReportingLocalTestingMode(
      {enabled: true}));

  testRunner.log(await dp.Storage.setAttributionReportingLocalTestingMode(
      {enabled: false}));

  testRunner.completeTest();
})
