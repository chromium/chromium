// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that an html import followed by a pending script does not break ` +
      `virtual time.`);

  const FetchHelper = await testRunner.loadScriptAbsolute(
      '../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  helper.setEnableLogging(false);
  await helper.enable();

  helper.onceRequest('http://test.com/index.html').fulfill(
      FetchHelper.makeContentResponse(`
          <!DOCTYPE html>
          <link rel="import" href="/import.html">
          <script src="/script.js"></script>
          <script type="module" src="/module.js"></script>`)
  );

  helper.onceRequest('http://test.com/import.html').fulfill(
      FetchHelper.makeContentResponse(`
          <script>console.log("imported html");</script>`)
  );

  helper.onceRequest('http://test.com/script.js').fulfill(
      FetchHelper.makeContentResponse(`
          console.log("ran script");`,
          "application/javascript")
  );

  helper.onceRequest('http://test.com/module.js').fulfill(
      FetchHelper.makeContentResponse(`
          console.log("ran module");`,
          "application/javascript")
  );

  await dp.Runtime.enable();
  await dp.Page.enable();

  let log_lines = [];
  dp.Runtime.onConsoleAPICalled(data => {
    log_lines.push(data.params.args[0].value);
  });

  dp.Emulation.onVirtualTimeBudgetExpired(async data => {
    testRunner.log(log_lines.sort());
    testRunner.completeTest();
  });

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending', budget: 5000,
      waitForNavigation: true});
  dp.Page.navigate({url: 'http://test.com/index.html'});
})
