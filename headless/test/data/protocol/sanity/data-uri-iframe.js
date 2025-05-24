// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests protocol events for a data-URI iframes');

  const {sessionId} =
      (await testRunner.browserP().Target.attachToBrowserTarget({})).result;
  const bp = (new TestRunner.Session(testRunner, sessionId)).protocol;

  const HttpInterceptor =
      await testRunner.loadScriptAbsolute('../resources/http-interceptor.js');
  const httpInterceptor = await (new HttpInterceptor(testRunner, bp)).init();

  httpInterceptor.addResponse('http://a.com/index.html', `<html></html>`);
  await session.navigate('http://a.com/index.html');

  dp.Page.enable();
  function logEvent(e) {
    const url = e.params.url || e.params.frame?.url || "";
    testRunner.log(`${e.method} ${url}`);
  }
  dp.Page.onFrameRequestedNavigation(logEvent);
  dp.Page.onFrameStartedNavigating(logEvent);
  dp.Page.onFrameDetached(() => {
    testRunner.log(`FAIL: ${e.method}`);
  });
  dp.Page.onFrameNavigated(logEvent);

  session.evaluate(`
      const iframe = document.createElement('iframe');
      iframe.src = 'data:text/html,<body></body>';
      document.body.appendChild(iframe);
  `);

  await dp.Page.onceFrameNavigated();

  testRunner.completeTest();
})
