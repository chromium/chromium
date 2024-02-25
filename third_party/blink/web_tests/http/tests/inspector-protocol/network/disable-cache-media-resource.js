(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that disable cache works for media resources.\n`);
  await dp.Network.enable();
  await dp.Network.setCacheDisabled({cacheDisabled: true});

  function createMediaFinishedPromise() {
    return new Promise(resolve => {
      const requestIdToType = {};
      dp.Network.onRequestWillBeSent(request => {
        requestIdToType[request.params.requestId] = request.params.type;
      });
      dp.Network.onLoadingFinished(finished => {
        if (requestIdToType[finished.params.requestId] === 'Media')
          resolve();
      });
    });
  }

  await page.navigate('.');
  const mediaFinishedPromise = createMediaFinishedPromise();
  await session.evaluate(`
    const video = document.createElement('video');
    video.src = './resources/flower.webm';
    document.body.appendChild(video);
    video.play();`);
  await mediaFinishedPromise;
  testRunner.log('got first media request');

  testRunner.log('reloading page');
  await dp.Page.reload();

  const mediaRequestPromise = new Promise(resolve => {
    dp.Network.onRequestWillBeSent(request => {
      if (request.params.type === 'Media')
        resolve();
    });
  });
  session.evaluate(`
    const video = document.createElement('video');
    video.src = './resources/flower.webm';
    document.body.appendChild(video);
    video.play();`);
  await mediaRequestPromise;
  testRunner.log('got second media request');

  testRunner.completeTest();
})
