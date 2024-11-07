// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async testRunner => {
    // This test requires kEnablePortBoundCookies to be enabled in order to pass.
    const {page, session, dp} = await testRunner.startBlank(
        `Verifies that Cookie blocks when ports do not match.\n`);

    await dp.Network.enable();
    await dp.Audits.enable();

    // Set the cookie.
    const response = await dp.Network.setCookie({
      url: 'https://example.test:10',
      secure: true,
      name: 'foo',
      value: 'bar',
      sameSite: 'None',
      sourcePort: 10,
    });

    if (response.error)
      testRunner.log(`setCookie failed: ${response.error.message}`);

    const setPortMismatch = 'https://example.test:8443/inspector-protocol/network/resources/hello-world.html';
    const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

    const {requestExtraInfo, responseExtraInfo} = await helper.navigateWithExtraInfo(setPortMismatch);
    testRunner.log(requestExtraInfo.params.associatedCookies, 'Cookie ports do not match:');
    testRunner.completeTest();
  });