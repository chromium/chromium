// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: double redirection.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  // Two navigations have been scheduled while the document was loading, but
  // only the second one was started. It canceled the first one.
  // Disable requested url logging because it is timing-dependent whether the
  // first load will reach the interceptor or not. The important thing is
  // that the final result shows the second url.
  httpInterceptor.setDisableRequestedUrlsLogging(true);
  httpInterceptor.addResponse('http://www.example.com/',
      `<html>
      <head>
        <title>Hello, World 1</title>
        <script>
          document.location='http://www.example.com/1';
          document.location='http://www.example.com/2';
        </script>
      </head>
      <body>http://www.example.com/1</body>
      </html>`);

  httpInterceptor.addResponse('http://www.example.com/1',
      '<p>Fail</p>');
  httpInterceptor.addResponse('http://www.example.com/2',
      '<p>Pass</p>');

  await virtualTimeController.initialize(1000);
  await frameNavigationHelper.navigate('http://www.example.com/');
  await virtualTimeController.grantTime(1000);
  testRunner.log(await session.evaluate('document.body.innerHTML'));
  frameNavigationHelper.logFrames();
  frameNavigationHelper.logScheduledNavigations();
  testRunner.completeTest();
})
