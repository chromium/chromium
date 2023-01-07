// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: redirect empty url.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse('http://www.example.com/',
      `<!DOCTYPE html><p>Pass</p>`,
      ['HTTP/1.1 302 Found',
       'Location: ',
       'Content-Type: text/html']);

  await virtualTimeController.initialize(1000);
  await frameNavigationHelper.navigate('http://www.example.com/');
  await virtualTimeController.grantTime(1000);

  testRunner.log(await session.evaluate('document.body.innerHTML'));
  testRunner.completeTest();
})
