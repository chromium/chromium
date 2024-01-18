(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      '../resources/test-page.html',
      `Tests that network instrumentation indicates HTTP 200 status for data: URLs.`);

  dp.Network.enable();
  dp.Page.enable();
  dp.Page.navigate({url: 'data:text/html,<div>Hello, world!</div>'});
  const navigationResponse = (await dp.Network.onceResponseReceived()).params.response;
  testRunner.log(`Navigation status: ${navigationResponse.status} (${navigationResponse.statusText})`);
  session.evaluate(`fetch('data:text/plain,Hello again!');`);
  const fetchResponse = (await dp.Network.onceResponseReceived()).params.response;
  testRunner.log(`Fetch status: ${fetchResponse.status} (${fetchResponse.statusText})`);
  testRunner.completeTest();
})
