// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: delayed completion.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse(
      `http://example.com/foobar`,
      `<html>
      <body>
       <script type="text/javascript">
         setTimeout(() => {
           var div = document.getElementById('content');
           var p = document.createElement('p');
           p.textContent = 'delayed text';
           div.appendChild(p);
         }, 3000);
       </script>
        <div id="content"/>
      </body>
      </html>`);

  await virtualTimeController.initialize(1000);
  await frameNavigationHelper.navigate('http://example.com/foobar');
  await virtualTimeController.grantTime(3000 + 100);
  testRunner.log(await session.evaluate(
    `document.getElementById('content').innerHTML.trim()`));
  testRunner.completeTest();

})
