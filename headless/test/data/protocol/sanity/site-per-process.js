// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests cross origin iframe with and withhout --site-per-process.');

  const {sessionId} =
      (await testRunner.browserP().Target.attachToBrowserTarget({})).result;
  const bp = (new TestRunner.Session(testRunner, sessionId)).protocol;

  const HttpInterceptor =
      await testRunner.loadScriptAbsolute('../resources/http-interceptor.js');
  const httpInterceptor = await (new HttpInterceptor(testRunner, bp)).init();
  httpInterceptor.setDisableRequestedUrlsLogging(true);

  httpInterceptor.addResponse('http://a.com/index.html', `<html></html>`);
  httpInterceptor.addResponse('http://b.com/index.html', `<html></html>`);

  await session.navigate('http://a.com/index.html');

  await session.evaluateAsync(() => new Promise(resolve => {
                                const iframe = document.createElement('iframe');
                                iframe.addEventListener(
                                    'load', resolve, {once: true});
                                iframe.src = 'http://b.com/index.html';
                                document.body.appendChild(iframe);
                              }));

  const {targetInfos} =
      (await dp.Target.getTargets({filter: [{type: 'iframe'}]})).result;

  if (testRunner.params('sitePerProcessEnabled')) {
    if (targetInfos[0].url === 'http://b.com/index.html') {
      testRunner.log('PASS');
    } else {
      testRunner.log(targetInfos);
    }
  } else {
    if (targetInfos.length == 0) {
      testRunner.log('PASS');
    } else {
      testRunner.log(targetInfos);
    }
  }

  testRunner.completeTest();
})
