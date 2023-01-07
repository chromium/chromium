// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests virtual time with same document history navigation.`);

  const FetchHelper = await testRunner.loadScriptAbsolute(
      '../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  await helper.enable();

  helper.onRequest('http://test.com/').fulfill(
      FetchHelper.makeContentResponse(`
        <body onload="step1()">
        <script>
          function step1() {
            // Step 1 - create some history by navigating forward.  Note
            // that this doesn't cause a load.
            history.pushState({}, '', '/foo');
            setTimeout(step2, 100);
          }

          function step2() {
            if (location.href !== 'http://test.com/foo')
              throw 'pushState failed.';
            history.back();
            setTimeout(step3, 100);
          }

          function step3() {
            if (location.href !== 'http://test.com/')
              throw 'Backward navigation failed.';
            history.forward();
            setTimeout(step4, 100);
          }

          function step4() {
            if (location.href !== 'http://test.com/foo')
              throw 'Forward navigation failed.';
            console.log('pass');
          }
        </script>
        </body>`)
  );

  await dp.Runtime.enable();
  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Page.navigate({url: 'http://test.com/'});
  dp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending', budget: 5000});
  const {params} = await dp.Runtime.onceConsoleAPICalled();
    testRunner.log(`PAGE: ${params.args[0].value}`);
    testRunner.completeTest();
})
