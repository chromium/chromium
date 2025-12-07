(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
    '../resources/test-page.html',
    `Verifies that cached cross-origin responses contain correct headers\n`);

  await dp.Network.enable();

  dp.Network.onResponseReceivedExtraInfo(event => {
    testRunner.log('response code=' + event.params.statusCode);
  });

  async function fetch() {
    await session.evaluateAsync(async (url) => {
      await fetch(url);
    }, testRunner.url('http://devtools.oopif.test:8000/inspector-protocol/network/resources/cached-revalidate.php'));
  }

  await fetch(); // initial response is a 200.
  await fetch(); // subsequent response is a 304.

  testRunner.completeTest();
})
