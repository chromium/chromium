// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const VirtualTimeController =
      await testRunner.loadScript('virtual-time-controller.js');

  const {page, session, dp} = await testRunner.startWithFrameControl(
    `Tests virtual time controller operation.`);

  await dp.Runtime.enable();
  await dp.HeadlessExperimental.enable();

  dp.Runtime.onConsoleAPICalled(data => {
    const text = data.params.args[0].value;
    testRunner.log(text);
  });

  const vtc = new VirtualTimeController(testRunner, dp, 25);
  await vtc.initialize(1000);
  testRunner.log(`onInstalled:`);
  await dp.Page.navigate({url: testRunner.url(
    'resources/virtual-time-controller-test.html')});
  for (let expirationCount = 0; expirationCount < 3; ++expirationCount) {
    const totalElapsedTime = await vtc.grantTime(
        expirationCount === 0 ? 100 : 50);
    testRunner.log(`onExpired: ${totalElapsedTime}`);
    if (expirationCount === 0)
      await session.evaluate('startRAF()');
  }
  testRunner.completeTest();
})
