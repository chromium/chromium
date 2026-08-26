// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests Browser.addMockCamera target scoping, parameter validation.');

  const pageTargetResponse =
      await dp.Browser.addMockCamera({deviceId: 'camera-1'});
  testRunner.log(
      `Page target rejected command: ${Boolean(pageTargetResponse.error)}`);

  const browserSession = await testRunner.attachFullBrowserSession();
  const browser = browserSession.protocol.Browser;

  const missingDeviceIdResponse = await browser.addMockCamera({});
  testRunner.log(
      `Missing deviceId rejected: ${Boolean(missingDeviceIdResponse.error)}`);

  const emptyDeviceIdResponse = await browser.addMockCamera({deviceId: ''});
  testRunner.log(
      `Empty deviceId rejected: ${Boolean(emptyDeviceIdResponse.error)}`);

  await browserSession.disconnect();
  testRunner.completeTest();
})
