// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: frames redirection chain.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse('http://www.example.com/', null,
      ['HTTP/1.1 302 Found', 'Location: http://www.example.com/1']);

  httpInterceptor.addResponse('http://www.example.com/1',
      `<html>
        <frameset>
          <frame id="frameA" src='http://www.example.com/frameA/'>
          <frame id="frameB" src='http://www.example.com/frameB/'>
        </frameset>
      </html>`);

  // Frame A
  httpInterceptor.addResponse('http://www.example.com/frameA/',
      `<html>
        <head>
          <script>document.location='http://www.example.com/frameA/1'</script>
        </head>
        <body>HELLO WORLD 1</body>
      </html>`);

  httpInterceptor.addResponse('http://www.example.com/frameA/1', null,
      ['HTTP/1.1 301 Moved', 'Location: /frameA/2']);

  httpInterceptor.addResponse('http://www.example.com/frameA/2',
      `<p>FRAME A</p>`);

  // Frame B
  httpInterceptor.addResponse('http://www.example.com/frameB/',
      `<html>
        <head><title>HELLO WORLD 2</title></head>
        <body>
          <iframe id="iframe" src='http://www.example.com/iframe/'></iframe>
        </body>
      </html>`);

  httpInterceptor.addResponse('http://www.example.com/iframe/',
      `<html>
        <head>
          <script>document.location='http://www.example.com/iframe/1'</script>
        </head>
        <body>HELLO WORLD 1</body>
      </html>`);

  httpInterceptor.addResponse('http://www.example.com/iframe/1', null,
      ['HTTP/1.1 302 Found', 'Location: /iframe/2']);

  httpInterceptor.addResponse('http://www.example.com/iframe/2', null,
      ['HTTP/1.1 301 Moved', 'Location: 3']);

  httpInterceptor.addResponse('http://www.example.com/iframe/3',
      `<p>IFRAME B</p>`);

  // Frame redirection requests are handled in an arbitrary order, so disable
  // requested url logging to ensure test's stability.
  httpInterceptor.setDisableRequestedUrlsLogging(true);

  await virtualTimeController.initialize(1000);
  await frameNavigationHelper.navigate('http://www.example.com/');
  await virtualTimeController.grantTime(1000);
  testRunner.log(await session.evaluate(
    `document.getElementById('frameA').contentDocument.body.innerHTML`));
  testRunner.log(await session.evaluate(
    `document.getElementById('frameB').contentDocument.` +
    `getElementById('iframe').contentDocument.body.innerHTML`));

  frameNavigationHelper.logFrames();
  frameNavigationHelper.logScheduledNavigations();

  httpInterceptor.hasRequestedUrls([
      'http://www.example.com/',
      'http://www.example.com/1',
      'http://www.example.com/frameA/',
      'http://www.example.com/frameA/1',
      'http://www.example.com/frameA/2',
      'http://www.example.com/frameB/',
      'http://www.example.com/iframe/',
      'http://www.example.com/iframe/1',
      'http://www.example.com/iframe/2',
      'http://www.example.com/iframe/3']);

  testRunner.completeTest();
})
