// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: redirect non ascii url.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  // "中文" is 0xE4 0xB8 0xAD, 0xE6 0x96 0x87
  httpInterceptor.addResponse('http://www.example.com/', null,
      ['HTTP/1.1 302 Found',
       'Location: http://www.example.com/%E4%B8%AD%E6%96%87']);

  httpInterceptor.addResponse('http://www.example.com/%E4%B8%AD%E6%96%87', null,
      ['HTTP/1.1 303 Moved',
       'Location: http://www.example.com/pass#%E4%B8%AD%E6%96%87']);

  httpInterceptor.addResponse('http://www.example.com/pass#%E4%B8%AD%E6%96%87',
      `<p>Pass</p>`);

  httpInterceptor.addResponse(
      'http://www.example.com/%C3%A4%C2%B8%C2%AD%C3%A6%C2%96%C2%87',
      `Fail`,
      ['HTTP/1.1 500 Bad Response', 'Content-Type: text/html']);

  await virtualTimeController.initialize(1000);
  await frameNavigationHelper.navigate('http://www.example.com/');
  await virtualTimeController.grantTime(1000);

  testRunner.log(await session.evaluate('document.body.innerHTML'));
  testRunner.completeTest();
})
