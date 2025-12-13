// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startWithFrameControl(`Tests that worker start aborted` +
         ` due to CDP does not block virtual time`);

  const RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  const {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse(`http://www.example.com/`, `
      <!doctype html>
      <meta http-equiv="Content-Security-Policy"
          content="default-src 'unsafe-inline'">
      <script type="text/javascript">
      const blob = new Blob([""], {type: "text/javascript"});
      const worker = new Worker(URL.createObjectURL(blob), {name: ""});
      </script>`);

  await virtualTimeController.initialize(1000);
  await frameNavigationHelper.navigate('http://www.example.com/');
  await virtualTimeController.grantTime(500);
  testRunner.log(await session.evaluate('document.body.innerHTML'));
  testRunner.completeTest();
})
