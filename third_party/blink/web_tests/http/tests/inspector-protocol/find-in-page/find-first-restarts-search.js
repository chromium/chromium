(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that FindInPage.findFirst restarts the search session with a ' +
      'new query, even while a previous find* session is still active.');
  const {waitForScroll} =
      await testRunner.loadScript('resources/scroll-helper.js');

  await session.evaluate(() => {
    document.body.style.margin = '0';
    document.body.innerHTML = `
      <div style="height:3000px"></div>
      <div>alpha</div>
      <div style="height:3000px"></div>
      <div>beta</div>
      <div style="height:3000px"></div>
      <div>alpha</div>
      <div style="height:3000px"></div>
      <div>beta</div>
      <div style="height:3000px"></div>
    `;
  });

  let scrollYPromise = waitForScroll(session);
  await dp.FindInPage.findFirst({query: 'alpha'});
  const firstAlphaY = await scrollYPromise;

  scrollYPromise = waitForScroll(session);
  await dp.FindInPage.findNext();
  const secondAlphaY = await scrollYPromise;
  testRunner.log(`findNext moved to the second alpha match: ${
      secondAlphaY > firstAlphaY}`);

  // findFirst() restarts the session (fresh match count, new session id in
  // FindRequestManager), but the renderer keeps searching forward from where
  // the previous session left off, so it should find the second 'beta'
  // occurrence.
  scrollYPromise = waitForScroll(session);
  const restartResponse = await dp.FindInPage.findFirst({query: 'beta'});
  const secondBetaY = await scrollYPromise;
  testRunner.log(`findFirst with a new query succeeded: ${
      !restartResponse.error}`);
      testRunner.log(
          `findFirst found 'beta' forward from the alpha session's ` +
          `position, not the first 'beta': ${secondBetaY >= secondAlphaY}`);

      // With only two 'beta' matches, findNext() from the second one wraps
      // around to the first.
      scrollYPromise = waitForScroll(session);
      await dp.FindInPage.findNext();
      const firstBetaY = await scrollYPromise;
      testRunner.log(`findNext wrapped around to the first 'beta': ${
          firstBetaY < secondBetaY}`);

      testRunner.completeTest();
})
