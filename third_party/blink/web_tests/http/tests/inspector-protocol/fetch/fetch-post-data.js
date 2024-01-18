(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Test post data interception`);

  await dp.Network.enable();
  await dp.Fetch.enable();

  const [requestWillBeSent, requestPaused] = await Promise.all([
    dp.Network.onceRequestWillBeSent(),
    dp.Fetch.onceRequestPaused(),
    session.evaluate(`
      const arr = [];
      for (let i = 0; i < 256; ++i)
        arr.push(i);
      fetch('${testRunner.url('./resources/hello-world.txt')}', {
          method: 'post',
          body: new Uint8Array(arr)
      })`)
  ]);

  printBytes(requestWillBeSent.params.request.postDataEntries[0].bytes);
  printBytes(requestPaused.params.request.postDataEntries[0].bytes);
  testRunner.completeTest();
  function printBytes(data) {
    const str = atob(data);
    const tokens = [];
    for (let i = 0; i < str.length; ++i)
      tokens.push(str.charCodeAt(i));
    testRunner.log(tokens);
  }
})
