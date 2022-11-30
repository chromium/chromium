// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: server redirection chain.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse('http://www.example.com/', null,
      ['HTTP/1.1 302 Found', 'Location: http://www.example.com/1']);

  httpInterceptor.addResponse('http://www.example.com/1', null,
      ['HTTP/1.1 301 Moved', 'Location: http://www.example.com/2']);

  httpInterceptor.addResponse('http://www.example.com/2', null,
      ['HTTP/1.1 302 Found', 'Location: http://www.example.com/3']);

  httpInterceptor.addResponse('http://www.example.com/3',
      '<p>Pass</p>');

  await virtualTimeController.initialize(1000);
  await frameNavigationHelper.navigate('http://www.example.com/');
  await virtualTimeController.grantTime(1000);

  testRunner.log(await session.evaluate('document.body.innerHTML'));
  testRunner.completeTest();
})
