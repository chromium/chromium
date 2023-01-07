// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that virtual time survives cross-process navigation.`);

  const FetchHelper = await testRunner.loadScriptAbsolute(
      '../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  await helper.enable();

  let virtualTime = 0;

  dp.Fetch.onRequestPaused(() => testRunner.log(`@ ${virtualTime}`));

  helper.onceRequest('http://a.com/').fulfill(
      FetchHelper.makeContentResponse(`
          <script>
          setTimeout(function() {
            window.location.href = "http://b.com/";
          }, 1000);
          </script>`)
  );

  helper.onceRequest('http://b.com/').fulfill(
      FetchHelper.makeContentResponse(`
          <script>
          setTimeout(function() {
            window.location.href = "http://c.com/";
          }, 1000);
          </script>`)
  );

  helper.onceRequest('http://c.com/').fulfill(
      FetchHelper.makeContentResponse(`
          <script>
          setTimeout(function() {
            window.location.href = "http://d.com/";
          }, 1000);
          </script>`)
  );

  helper.onceRequest('http://d.com/').fulfill(
    FetchHelper.makeContentResponse(`<html></html>`)
  );

  const virtualTimeBudget = 100;

  let count = 0;
  dp.Emulation.onVirtualTimeBudgetExpired(data => {
    virtualTime += virtualTimeBudget;
    if (++count < 50) {
      dp.Emulation.setVirtualTimePolicy({
          policy: 'pauseIfNetworkFetchesPending', budget: virtualTimeBudget});
    } else {
      testRunner.completeTest();
    }
  });

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  dp.Page.navigate({url: 'http://a.com'});
  await dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending', budget: virtualTimeBudget});
})
