// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank('Tests focus/blur notifications.');

  await dp.Runtime.enable();

  dp.Runtime.onConsoleAPICalled(data => {
    const text = data.params.args[0].value;
    testRunner.log(text);
  });

  await dp.Page.enable();
  dp.Page.navigate(
      {url: testRunner.url('/resources/focus-blur-notifications.html')});
  await dp.Page.onceLoadEventFired();

  testRunner.completeTest();
})
