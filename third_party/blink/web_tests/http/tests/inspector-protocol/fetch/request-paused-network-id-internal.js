(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'resources/hello-world.html',
      `Verifies that requestPaused does not have a networkId for internal requests`);

  await dp.Fetch.enable();

  dp.DOM.enable();
  // This forces an internal request to read the html from the cache.
  dp.CSS.enable();

  const requestPaused = await dp.Fetch.onceRequestPaused();
  testRunner.log(`requestPaused.url: ${requestPaused.params.request.url}`);
  testRunner.log(`requestPaused.networkId: ${requestPaused.params.networkId}`);
  testRunner.completeTest();
})
