// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests popup window open placement.');

  const {sessionId} =
      (await testRunner.browserP().Target.attachToBrowserTarget({})).result;
  const bp = (new TestRunner.Session(testRunner, sessionId)).protocol;

  const HttpInterceptor =
      await testRunner.loadScriptAbsolute('../resources/http-interceptor.js');
  const httpInterceptor = await (new HttpInterceptor(testRunner, bp)).init();
  httpInterceptor.setDisableRequestedUrlsLogging(true);

  httpInterceptor.addResponse('https://example.com/index.html', `
      <script>
          const popup = window.open('/page2.html', '_blank',
              'popup, left=10, top=20, width=400, height=200');
          popup.addEventListener('load', async () => {
            console.log('Popup: ' +
                '{' + popup.screenLeft + ',' + popup.screenTop +
                ' ' + popup.innerWidth + 'x' + popup.innerHeight +
                '}');
          });
      </script>`);

  httpInterceptor.addResponse(
      'https://example.com/page2.html', `<body>Page2</body>`);

  await dp.Browser.grantPermissions({permissions: ['windowManagement']});

  dp.Runtime.enable();
  const readyPromise = dp.Runtime.onceConsoleAPICalled();

  session.navigate('https://example.com/index.html');

  const message = (await readyPromise).params.args[0].value;
  testRunner.log(message);

  testRunner.completeTest();
})
