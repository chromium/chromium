// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests screen rotation angle.');

  const HttpInterceptor =
      await testRunner.loadScriptAbsolute('../resources/http-interceptor.js');
  const httpInterceptor = await (new HttpInterceptor(testRunner, dp)).init();

  httpInterceptor.setDisableRequestedUrlsLogging(true);
  httpInterceptor.addResponse(
      'https://example.com/index.html', `<html></html>`);

  await dp.Browser.grantPermissions({permissions: ['windowManagement']});

  await session.navigate('https://example.com/index.html');

  const result = await session.evaluateAsync(async () => {
    const cs = (await getScreenDetails()).currentScreen;
    return `Screen: ${cs.left},${cs.top} ${cs.width}x${cs.height}\n` +
        `angle: ${cs.orientation.angle}`;
  });

  testRunner.log(result);

  testRunner.completeTest();
})
