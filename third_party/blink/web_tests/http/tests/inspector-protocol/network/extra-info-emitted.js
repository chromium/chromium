(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
    `Verifies that certain types of requests have or don't have Network.*ExtraInfo events, and makes sure that responseReceived.hasExtraInfo matches the presence of the ExtraInfo events.\n`);

  await dp.Network.enable();
  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  async function assertNoExtraInfo(url) {
    const responseReceivedPromise = dp.Network.onceResponseReceived();
    await session.evaluate(`fetch('${url}')`);
    const responseReceived = await responseReceivedPromise;
    testRunner.log(`fetched subresource: ${url}`);
    testRunner.log(`responseReceived.url: ${responseReceived.params.response.url}`);
    testRunner.log(`responseReceived.hasExtraInfo: ${responseReceived.params.hasExtraInfo}`);
    testRunner.log('');
  }

  async function assertHasExtraInfo(url) {
    session.evaluate(
        `fetch('${url}', {method: 'POST', credentials: 'include'})`);
    const [responseReceived, requestExtraInfo, responseExtraInfo] =
        await Promise.all([
          dp.Network.onceResponseReceived(),
          dp.Network.onceRequestWillBeSentExtraInfo(),
          dp.Network.onceResponseReceivedExtraInfo()
        ]);
    testRunner.log(`fetched subresource: ${url}`);
    testRunner.log(`responseReceived.url: ${responseReceived.params.response.url}`);
    testRunner.log(`responseReceived.hasExtraInfo: ${responseReceived.params.hasExtraInfo}`);
    testRunner.log(`requestWillBeSentExtraInfo present: ${requestExtraInfo.params.requestId === responseReceived.params.requestId}`);
    testRunner.log(`responseReceivedExtraInfo present: ${responseExtraInfo.params.requestId === responseReceived.params.requestId}`);
    testRunner.log('');
  }

  async function assertNoRequest(url) {
    const navigatedPromise = dp.Page.onceFrameNavigated();
    dp.Network.onResponseReceived(() => {
      testRunner.log(`Unexpected network response received`);
    });
    await session.navigate(url);
    await navigatedPromise;
    testRunner.log(`navigated to: ${url}`);
    testRunner.log('');
  }

  async function assertHasExtraInfoNavigation(url) {
    const responseReceivedPromise = dp.Network.onceResponseReceived();
    const {requestExtraInfo, responseExtraInfo} = await helper.navigateWithExtraInfo(url);
    const responseReceived = await responseReceivedPromise;
    testRunner.log(`navigated to: ${url}`);
    testRunner.log(`responseReceived.url: ${responseReceived.params.response.url}`);
    testRunner.log(`responseReceived.hasExtraInfo: ${responseReceived.params.hasExtraInfo}`);
    testRunner.log(`requestWillBeSentExtraInfo present: ${requestExtraInfo.params.requestId === responseReceived.params.requestId}`);
    testRunner.log(`responseReceivedExtraInfo present: ${responseExtraInfo.params.requestId === responseReceived.params.requestId}`);
    testRunner.log('');
  }

  await assertNoExtraInfo(`data:text/plain,helloWorld`);
  await assertNoExtraInfo(`data:text/html,helloWorld`);
  await assertNoExtraInfo(`data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==`);
  await assertHasExtraInfo('/');

  await assertHasExtraInfoNavigation('/');
  await assertHasExtraInfoNavigation('data:text/html,<div>helloWorld</div>');
  await assertNoRequest('about:blank');

  // TODO can I also test file urls in web_tests...?

  testRunner.completeTest();
})
