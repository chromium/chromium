// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: override title with JavaScript enabled.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  await dp.Emulation.setScriptExecutionDisabled({value: false});

  httpInterceptor.addResponse(
      `http://example.com/foobar`,
      `<html>
        <head>
          <title>JavaScript is off</title>
          <script language="JavaScript">
            function settitle() {
              document.title = 'JavaScript is on';
            }
            </script>
          </head>
        <body onload="settitle()">
          Hello, World!
        </body>
      </html>`);


  await virtualTimeController.initialize(1000);
  await frameNavigationHelper.navigate('http://example.com/foobar');
  await virtualTimeController.grantTime(500);

  testRunner.log(await session.evaluate('document.title'));
  testRunner.completeTest();
})
