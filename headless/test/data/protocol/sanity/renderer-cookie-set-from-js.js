// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: cookie set from js.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse(
      `http://www.example.com/`,
      `<html>
        <head>
          <script>
            document.cookie = 'SessionID=123';
          </script>
        </head>
        <body>Hello, World!</body>
      </html>`);

  await dp.Emulation.setDocumentCookieDisabled({disabled: false});

  await virtualTimeController.initialize(1000);
  await frameNavigationHelper.navigate('http://www.example.com/');
  await virtualTimeController.grantTime(5000);

  const cookieIndex =
      await session.evaluate(`document.cookie.indexOf('SessionID')`);
  testRunner.log(cookieIndex < 0 ? 'FAIL' : 'pass');
  testRunner.completeTest();
})
