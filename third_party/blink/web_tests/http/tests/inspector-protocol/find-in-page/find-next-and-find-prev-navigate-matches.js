(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that FindInPage.findNext/findPrev move between matches.');
  const {waitForScroll} =
      await testRunner.loadScript('resources/scroll-helper.js');

  await session.evaluate(() => {
    document.body.style.margin = '0';
    document.body.innerHTML = `
      <div style="height:3000px"></div>
      <div>needle</div>
      <div style="height:3000px"></div>
      <div>needle</div>
      <div style="height:3000px"></div>
    `;
  });

  const initialY = await session.evaluate(() => window.scrollY);

  let scrollYPromise = waitForScroll(session);
  await dp.FindInPage.findFirst({query: 'needle'});
  const firstMatchY = await scrollYPromise;
  testRunner.log(
      `findFirst scrolled to the first match: ${firstMatchY > initialY}`);

  scrollYPromise = waitForScroll(session);
  await dp.FindInPage.findNext();
  const secondMatchY = await scrollYPromise;
  testRunner.log(`findNext scrolled further down to the second match: ${
      secondMatchY > firstMatchY}`);

  scrollYPromise = waitForScroll(session);
  await dp.FindInPage.findPrev();
  const backToFirstMatchY = await scrollYPromise;
  testRunner.log(`findPrev scrolled back up towards the first match: ${
      backToFirstMatchY < secondMatchY}`);

  testRunner.completeTest();
})
