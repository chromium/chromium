// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests screen details pixel ratio and color depth.');

  const HttpInterceptor =
      await testRunner.loadScriptAbsolute('../resources/http-interceptor.js');
  const httpInterceptor = await (new HttpInterceptor(testRunner, dp)).init();

  httpInterceptor.setDisableRequestedUrlsLogging(true);
  httpInterceptor.addResponse(
      'https://example.com/index.html', `<html></html>`);

  await dp.Browser.grantPermissions({permissions: ['windowManagement']});

  await session.navigate('https://example.com/index.html');

  const result = await session.evaluateAsync(async () => {
    const screenDetails = await getScreenDetails();
    const screenInfos = screenDetails.screens.map(
        s => `${s.label}: colorDepth=${s.colorDepth} devicePixelRatio=${
            s.devicePixelRatio}`);
    return screenInfos.join('\n');
  });

  testRunner.log(result);

  testRunner.completeTest();
})
