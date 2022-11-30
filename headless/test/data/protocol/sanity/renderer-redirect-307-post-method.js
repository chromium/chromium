// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: redirection 307 post method.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse('http://www.example.com/',
      `<html>
        <body onload='document.forms[0].submit();'>
          <form action='1' method='post'>
            <input name='foo' value='bar'>
          </form>
          </body>
      </html>`);

  httpInterceptor.addResponse('http://www.example.com/1', null,
      ['HTTP/1.1 307 Temporary Redirect', 'Location: /2']);

  httpInterceptor.addResponse('http://www.example.com/2',
      '<p>Pass</p>');

  await virtualTimeController.initialize(1000);
  await frameNavigationHelper.navigate('http://www.example.com/');
  await virtualTimeController.grantTime(1000);

  testRunner.log(await session.evaluate('document.body.innerHTML'));
  httpInterceptor.logRequestedMethods();
  frameNavigationHelper.logFrames();
  frameNavigationHelper.logScheduledNavigations();
  testRunner.completeTest();
})
