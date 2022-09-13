// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: redirect base url.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse('http://foo.com/', null,
      ['HTTP/1.1 302 Found', 'Location: http://bar.com/']);

  httpInterceptor.addResponse('http://bar.com/',
      `<p>Pass</p>`);

  await virtualTimeController.initialize(1000);
  await frameNavigationHelper.navigate('http://foo.com/');
  await virtualTimeController.grantTime(1000);

  testRunner.log(await session.evaluate('document.body.innerHTML'));
  testRunner.completeTest();
})
