// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: mixed redirection chain.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse('http://www.example.com/',
      `<html>
        <head>
          <meta http-equiv="refresh" content="0; url=http://www.example.com/1"/>
          <title>Hello, World 0</title>
        </head>
        <body>http://www.example.com/</body>
      </html>`);

  httpInterceptor.addResponse('http://www.example.com/1',
      `<html>
        <head>
          <title>Hello, World 1</title>
          <script>
            document.location='http://www.example.com/2';
          </script>
        </head>
        <body>http://www.example.com/1</body>
      </html>`);

  httpInterceptor.addResponse('http://www.example.com/2', null,
      ['HTTP/1.1 302 Found', 'Location: 3']);

  httpInterceptor.addResponse('http://www.example.com/3', null,
      ['HTTP/1.1 301 Moved', 'Location: http://www.example.com/4']);

  httpInterceptor.addResponse('http://www.example.com/4',
      `<p>Pass</p>`);

  await virtualTimeController.initialize(1000);
  await frameNavigationHelper.navigate('http://www.example.com/');
  await virtualTimeController.grantTime(1000);

  testRunner.log(await session.evaluate('document.body.innerHTML'));
  testRunner.completeTest();
})
