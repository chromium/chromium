(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank('Tests the Emulation.setDisabledImageTypes method with a <picture> element.');

  await dp.Page.enable();
  await dp.Network.enable();

  let requestEvents = [];
  dp.Network.onRequestWillBeSent(event => requestEvents.push(event));

  await dp.Emulation.setDisabledImageTypes({ imageTypes: ['webp'] });

  testRunner.log('With emulation (avif enabled):');
  await page.navigate(testRunner.url('resources/image-fallback-picture.html'));
  testRunner.log('Expected avif image: ' + await session.evaluate(() => document.querySelector('img').currentSrc));
  const avifRequest = requestEvents.map(event => event.params.request).find(request => request.url.endsWith('test.avif'));
  testRunner.log('Image request Accept header: ' + avifRequest.headers.Accept);

  requestEvents = [];

  testRunner.log('With emulation (avif disabled):');
  await dp.Emulation.setDisabledImageTypes({ imageTypes: ['avif'] });
  dp.Page.reload({ ignoreCache: true });
  await dp.Page.onceLoadEventFired();
  testRunner.log('Expected png image: ' + await session.evaluate(() => document.querySelector('img').currentSrc));
  const pngRequest = requestEvents.map(event => event.params.request).find(request => request.url.endsWith('test.png'));
  testRunner.log('Image request Accept header: ' + pngRequest.headers.Accept);

  testRunner.completeTest();
})
