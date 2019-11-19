// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We want to pad |kLargeDotJS| out with some dummy code which is parsed
// asynchronously to make sure the virtual_time_pauser in PendingScript
// actually does something. We construct a large number of long unused
// function declarations which seems to trigger the desired code path.
const dummy = [];
for (let i = 0; i < 1024; ++i)
  dummy.push(`var i${i}=function(){return '${'A'.repeat(4096)}';}`);

const largeDotJS = `
(function() {
var setTitle = newTitle => document.title = newTitle;
${dummy.join('\n')}
setTitle('Test PASS');
})();`;

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that pending script does not break virtual time.`);

  const FetchHelper = await testRunner.loadScriptAbsolute(
      '../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  await helper.enable();

  helper.onceRequest('http://test.com/index.html').fulfill(
      FetchHelper.makeContentResponse(`
          <script src="/large.js"></script>`)
  );

  helper.onceRequest('http://test.com/large.js').fulfill(
      FetchHelper.makeContentResponse(largeDotJS,
          'application/javascript')
  );

  dp.Emulation.onVirtualTimeBudgetExpired(async data => {
    testRunner.log(await session.evaluate('document.title'));
    testRunner.completeTest();
  });

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending', budget: 5000,
      waitForNavigation: true});
  dp.Page.navigate({url: 'http://test.com/index.html'});
})
