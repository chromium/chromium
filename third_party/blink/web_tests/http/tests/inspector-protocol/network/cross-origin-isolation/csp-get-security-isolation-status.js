(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that CSP is reported along with isolation status');

  await dp.Page.enable();

  let frameId;
  dp.Page.onFrameNavigated((event) => {
    frameId = event.params.frame.id;
  });

  await page.navigate(
      'https://devtools.oopif.test:8443/inspector-protocol/network/cross-origin-isolation/resources/csp-set-in-meta.html');

  const {result} = await session.protocol.Network.getSecurityIsolationStatus({frameId});

  testRunner.log(result.status.csp, 'CSP status');

  testRunner.completeTest();
})

