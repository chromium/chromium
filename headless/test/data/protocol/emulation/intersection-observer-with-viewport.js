// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startWithFrameControl(
      `Tests that intersection observer works with device emulation.`);
  const RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  const {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  const params = {
      deviceScaleFactor: 1,
      width: 384,
      height: 800,
      mobile: true,
      screenWidth: 384,
      screenHeight: 800,
      viewport: {x: 0, y: 0, width: 1, height: 1, scale: 1}
  };
  await dp.Emulation.setDeviceMetricsOverride(params);
  dp.Runtime.enable();
  httpInterceptor.addResponse(`http://example.com/`, `
      <html><head>
      <style>
        .tall { background-color: green; height: 3000px; }
        .short { background-color: red; height: 100px; }
      </style></head>
      <body>
        <div id='target' class='short'></div>
        <div class='tall'></div>
      </body>
      <script>
        window.addEventListener('DOMContentLoaded', () => {
          const observer = new IntersectionObserver((entries, observer) => {
            for (const entry of entries) {
              console.log(entry.target.id + ' intersecting ' +
                  entry.isIntersecting);
            }
          });
          observer.observe(document.querySelector('#target'));
        });
      </script>
  `);
  await virtualTimeController.initialize(1000);
  await frameNavigationHelper.navigate('http://example.com/');
  await virtualTimeController.grantTime(1000);
  session.evaluateAsync(`
      window.scrollBy(0, 200);
  `);
  await virtualTimeController.grantTime(500);
  session.evaluateAsync(`
      window.scrollTo(0, 0);
  `);
  await virtualTimeController.grantTime(500);
  testRunner.completeTest();
})
