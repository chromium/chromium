// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: frame load events.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse(`http://example.com/`, null,
      ['HTTP/1.1 302 Found', 'Location: http://example.com/1']);

  httpInterceptor.addResponse(`http://example.com/1`,
      `<html><frameset>
        <frame id="frameA" src="http://example.com/frameA/">
        <frame id="frameB" src="http://example.com/frameB/">
      </frameset></html>`);

  httpInterceptor.addResponse(`http://example.com/frameA/`,
      `<html><head><script>
        document.location="http://example.com/frameA/1"
      </script></head></html>`);

  httpInterceptor.addResponse(`http://example.com/frameB/`,
      `<html><head><script>
        document.location="http://example.com/frameB/1"
      </script></head></html>`);

  httpInterceptor.addResponse(`http://example.com/frameA/1`,
      `<html><body>FRAME A 1</body></html>`);

  httpInterceptor.addResponse(`http://example.com/frameB/1`,
      `<html><body>FRAME B 1
        <iframe id="iframe" src="http://example.com/frameB/1/iframe/"></iframe>
      </body></html>`);

  httpInterceptor.addResponse(`http://example.com/frameB/1/iframe/`,
      `<html><head><script>
        document.location="http://example.com/frameB/1/iframe/1"
      </script></head></html>`);

  httpInterceptor.addResponse(`http://example.com/frameB/1/iframe/1`,
      `<html><body>IFRAME 1</body><html>`);

  // Frame redirection requests are handled in an arbitrary order, so disable
  // requested url logging to ensure test's stability.
  httpInterceptor.setDisableRequestedUrlsLogging(true);

  await virtualTimeController.initialize(1000);
  await frameNavigationHelper.navigate('http://example.com/');
  await virtualTimeController.grantTime(500);

  testRunner.log(await session.evaluate(
    `document.getElementById('frameA').contentDocument.body.innerText`));
  testRunner.log(await session.evaluate(
    `document.getElementById('frameB').contentDocument.` +
    `getElementById('iframe').contentDocument.body.innerHTML`));

  frameNavigationHelper.logFrames();
  frameNavigationHelper.logScheduledNavigations();

  httpInterceptor.hasRequestedUrls([
      'http://example.com/',
      'http://example.com/1',
      'http://example.com/frameA/',
      'http://example.com/frameA/1',
      'http://example.com/frameB/',
      'http://example.com/frameB/1',
      'http://example.com/frameB/1/iframe/',
      'http://example.com/frameB/1/iframe/1'
  ]);

  testRunner.completeTest();
})
