(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
    `Test post data interception`);

  await dp.Runtime.enable();
  await dp.Network.enable();
  await dp.Fetch.enable({ patterns: [{ urlPattern: '*' }] });

  session.evaluate(`
    fetch('${testRunner.url('./resources/post-echo.pl')}', {
      method: 'post',
      body: 'hello'
    }).then(r => r.text()).then(t => console.log(t))`);

  const requestPaused = await dp.Fetch.onceRequestPaused();
  const [event] = await Promise.all([
    dp.Runtime.onceConsoleAPICalled(),
    dp.Fetch.continueRequest({
      requestId: requestPaused.params.requestId,
      postData: btoa('binary string')
    }),
  ]);
  testRunner.log(event.params.args[0].value);

  session.evaluate(`
    fetch('${testRunner.url('./resources/post-echo.pl')}', {
      method: 'post',
      body: 'hello'
    })`);
  const requestPaused2 = await dp.Fetch.onceRequestPaused();
  const {error} = await dp.Fetch.continueRequest({
    requestId: requestPaused2.params.requestId,
    postData: '¯\_(ツ)_/¯ not a base64 string ¯\_(ツ)_/¯'});

  function trimErrorMessage(message) {
    return message.replace(/at position \d+/, "<somewhere>");
  }
  testRunner.log(`error when passing body as string: ${trimErrorMessage(error.data)}`);

  testRunner.completeTest();
})
