(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank('Tests the Emulation.setDisabledImageTypes method for JPEG XL.');

  await dp.Page.enable();
  await dp.Network.enable();

  let requestEvents = [];
  dp.Network.onRequestWillBeSent(event => requestEvents.push(event));

  await dp.Emulation.setDisabledImageTypes({ imageTypes: ['webp'] });

  testRunner.log('With emulation (jxl enabled):');
  await page.navigate(testRunner.url('resources/image-jxl-fallback-img.html'));
  testRunner.log('Expected jxl image: ' + await session.evaluate(() => document.querySelector('img').currentSrc));
  let jxlRequest = requestEvents.map(event => event.params.request).find(request => request.url.endsWith('test.jxl'));
  testRunner.log('Image request Accept header: ' + jxlRequest.headers.Accept);

  requestEvents = [];

  testRunner.log('With emulation (jxl disabled):');
  await dp.Emulation.setDisabledImageTypes({ imageTypes: ['jxl'] });
  dp.Page.reload({ ignoreCache: true });
  await dp.Page.onceLoadEventFired();
  testRunner.log('Expected png image: ' + await session.evaluate(() => document.querySelector('img').currentSrc));
  let pngRequest = requestEvents.map(event => event.params.request).find(request => request.url.endsWith('test.png'));
  testRunner.log('Image request Accept header: ' + pngRequest.headers.Accept);

  requestEvents = [];

  await dp.Emulation.setDisabledImageTypes({ imageTypes: ['webp'] });

  testRunner.log('With emulation (jxl enabled):');
  await page.navigate(testRunner.url('resources/image-jxl-fallback-picture.html'));
  testRunner.log('Expected jxl image: ' + await session.evaluate(() => document.querySelector('img').currentSrc));
  jxlRequest = requestEvents.map(event => event.params.request).find(request => request.url.endsWith('test.jxl'));
  testRunner.log('Image request Accept header: ' + jxlRequest.headers.Accept);

  requestEvents = [];

  testRunner.log('With emulation (jxl disabled):');
  await dp.Emulation.setDisabledImageTypes({ imageTypes: ['jxl'] });
  dp.Page.reload({ ignoreCache: true });
  await dp.Page.onceLoadEventFired();
  testRunner.log('Expected png image: ' + await session.evaluate(() => document.querySelector('img').currentSrc));
  pngRequest = requestEvents.map(event => event.params.request).find(request => request.url.endsWith('test.png'));
  testRunner.log('Image request Accept header: ' + pngRequest.headers.Accept);

  testRunner.completeTest();
})