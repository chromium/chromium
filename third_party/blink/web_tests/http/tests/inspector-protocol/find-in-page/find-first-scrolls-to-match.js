(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that FindInPage.findFirst scrolls the match into view.');
  const {waitForScroll} =
      await testRunner.loadScript('resources/scroll-helper.js');

  await session.evaluate(() => {
    document.body.style.margin = '0';
    document.body.innerHTML = `
      <div style="height:3000px"></div>
      <div id="target">needle</div>
      <div style="height:3000px"></div>
    `;
  });

  const before = await session.evaluate(() => window.scrollY);
  testRunner.log(`Initial scrollY is zero: ${before === 0}`);

  const scrollYPromise = waitForScroll(session);
  const response = await dp.FindInPage.findFirst({query: 'needle'});
  testRunner.log(`findFirst succeeded: ${!response.error}`);

  const after = await scrollYPromise;
  testRunner.log(`Page scrolled down to reveal the match: ${after > before}`);

  testRunner.completeTest();
})
