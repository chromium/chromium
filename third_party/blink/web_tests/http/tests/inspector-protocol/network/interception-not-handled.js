(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that browser does not crash or hit DCHECK() when an intercepted request is abandoned.`);
  await dp.Network.enable();
  await dp.Network.setRequestInterception({patterns: [{}]});
  dp.Page.navigate({url: 'http://a.com'});
  await dp.Network.onceRequestIntercepted();
  testRunner.completeTest();
})
