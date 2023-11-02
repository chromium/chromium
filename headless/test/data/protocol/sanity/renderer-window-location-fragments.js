// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: window location fragments.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse('http://www.example.com/#fragment1',
      `<script>
        if (window.location.hash == '#fragment1') {
          document.write('<iframe id="frame" src="iframe#fragment2"></iframe>');
        }
      </script>`);

  httpInterceptor.addResponse('http://www.example.com/iframe#fragment2',
      `<script>
        if (window.location.hash == '#fragment2') {
          document.location = 'http://www.example.com/pass';
        }
      </script>)`);

  httpInterceptor.addResponse('http://www.example.com/pass',
      `<p>Pass</p>`);

  await virtualTimeController.initialize(1000);
  await frameNavigationHelper.navigate('http://www.example.com/#fragment1');
  await virtualTimeController.grantTime(1000);

  testRunner.log(await session.evaluate(
    `document.getElementById('frame').contentDocument.body.innerText`));
  testRunner.completeTest();
})
