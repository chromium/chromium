(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that Network.Response includes the charset`);

  await dp.Network.enable();
  await dp.Network.setCacheDisabled({cacheDisabled: true});
  testRunner.log('Network Enabled');

  {
    testRunner.log('Fetching with no charset');
    session.evaluate(`fetch('/inspector-protocol/network/resources/content-type.php?contentType=${encodeURI('text/html')}');`);
    const event = await dp.Network.onceResponseReceived();
    testRunner.log(`Network.Response.charset: ${event.params.response.charset}`);
    await dp.Network.onceLoadingFinished();  // Prevent racing with other tests.
  }

  {
    testRunner.log('Fetching with utf-8');
    session.evaluate(`fetch('/inspector-protocol/network/resources/content-type.php?contentType=${encodeURI('text/html; charset=utf-8')}');`);
    const event = await dp.Network.onceResponseReceived();
    testRunner.log(`Network.Response.charset: ${event.params.response.charset}`);
    await dp.Network.onceLoadingFinished();  // Prevent racing with other tests.
  }

  {
    testRunner.log('Fetching with iso-8869-1');
    session.evaluate(`fetch('/inspector-protocol/network/resources/content-type.php?contentType=${encodeURI('text/html; charset=iso-8859-1')}');`);
    const event = await dp.Network.onceResponseReceived();
    testRunner.log(`Network.Response.charset: ${event.params.response.charset}`);
    await dp.Network.onceLoadingFinished();  // Prevent racing with other tests.
  }

  {
    testRunner.log('Fetching utf-16 content');
    session.evaluate(`fetch('/inspector-protocol/network/resources/content-type-utf16.php');`);
    const event = await dp.Network.onceResponseReceived();
    testRunner.log(`Network.Response.charset: ${event.params.response.charset}`);
    await dp.Network.onceLoadingFinished();
    const {result: {body}} = await dp.Network.getResponseBody({requestId: event.params.requestId});
    testRunner.log(`Response body: ${body}`);
  }
  testRunner.completeTest();
});
