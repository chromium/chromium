// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests window open on a secondary screen.');

  const {sessionId} =
      (await testRunner.browserP().Target.attachToBrowserTarget({})).result;
  const bp = (new TestRunner.Session(testRunner, sessionId)).protocol;

  const HttpInterceptor =
      await testRunner.loadScriptAbsolute('../resources/http-interceptor.js');
  const httpInterceptor = await (new HttpInterceptor(testRunner, bp)).init();
  httpInterceptor.setDisableRequestedUrlsLogging(true);

  httpInterceptor.addResponse('https://example.com/index.html', `
      <script>
          const win = window.open('/page2.html', '_blank',
              'left=820, top=20, width=400, height=200');
          win.addEventListener('load', async () => {
            const cs = (await win.getScreenDetails()).currentScreen;
            console.log('Page2 screen: ' + cs.label);
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
