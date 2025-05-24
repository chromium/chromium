// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests pop ups can be blocked.');

  const {sessionId} =
      (await testRunner.browserP().Target.attachToBrowserTarget({})).result;
  const bp = (new TestRunner.Session(testRunner, sessionId)).protocol;

  const HttpInterceptor =
      await testRunner.loadScriptAbsolute('../resources/http-interceptor.js');
  const httpInterceptor = await (new HttpInterceptor(testRunner, bp)).init();

  httpInterceptor.setDisableRequestedUrlsLogging(true);
  httpInterceptor.addResponse('http://example.com/index.html', `
      <script>
          const win = window.open('/page2.html');
          if (!win) {
              console.error('ready');
          }
          win.addEventListener('load', () => console.log('ready'));
      </script>`);

  httpInterceptor.addResponse('http://example.com/page2.html',
      `<body>Page 2</body>`);

  dp.Runtime.enable();
  const readyPromise = dp.Runtime.onceConsoleAPICalled();
  session.navigate('http://example.com/index.html');

  const message = (await readyPromise).params.args[0].value;
  if (message !== 'ready') {
    testRunner.fail(`Unexpected console message: ${message}`);
  }
  const requestedUrls = new Set(httpInterceptor.requestedUrls());
  if (!requestedUrls.has('http://example.com/index.html')) {
    testRunner.fail('Main page not requested');
  }
  const seenPopupRequest = requestedUrls.has(
      'http://example.com/page2.html');
  if (seenPopupRequest === !testRunner.params('blockingNewWebContents')) {
    testRunner.log('PASS');
  } else {
    const message = seenPopupRequest ? 'Popup blocked but requested'
                                     : 'Popup not blocked but not requested';
    testRunner.log(Array.from(requestedUrls), `FAIL: ${message}: `);
  }

  testRunner.completeTest();
})
