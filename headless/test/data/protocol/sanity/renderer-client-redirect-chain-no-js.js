// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: chained client redirection with js disabled.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse(
      `http://www.example.com/`,
      `<html>
        <head>
          <meta http-equiv="refresh" content="0; url=http://www.example.com/1"/>
          <title>Hello, World 0</title>
        </head>
        <body>http://www.example.com/</body>
      </html>`);

  httpInterceptor.addResponse(
      `http://www.example.com/1`,
      `<html>
        <head>
          <title>Hello, World 1</title>
          <script>
            document.location='http://www.example.com/2';
          </script>
        </head>
        <body>http://www.example.com/1</body>
      </html>`);

  httpInterceptor.addResponse(
      `http://www.example.com/2`,
      `<html>
        <head>
          <title>Hello, World 2</title>
          <script>
            setTimeout("document.location='http://www.example.com/3'", 1000);
          </script>
        </head>
        <body>http://www.example.com/2</body>
      </html>`);

  httpInterceptor.addResponse(
      `http://www.example.com/3`,
      `<html>
        <head>
          <title>Pass</title>
        </head>
        <body>
          http://www.example.com/3
          <img src="pass">
        </body>
      </html>`);

  httpInterceptor.addResponse(
      `http://www.example.com/pass`,
      `<html>
        <body>
        pass
        </body>
      </html>`);

  await dp.Emulation.setScriptExecutionDisabled({value: true});
  await virtualTimeController.initialize(1000);
  await frameNavigationHelper.navigate('http://www.example.com/');
  await virtualTimeController.grantTime(1000 + 100);

  testRunner.log(await session.evaluate('document.title'));
  frameNavigationHelper.logFrames();
  frameNavigationHelper.logScheduledNavigations();
  testRunner.completeTest();
})
