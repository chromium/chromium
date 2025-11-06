(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank('Verifies Network.setBlockedURLs API.');

  await dp.Network.enable();
  await dp.Runtime.enable();
  await dp.Network.setCacheDisabled({cacheDisabled: true});

  /**
   * @param {string} path
   * @returns {boolean}
   */
  async function isBlocked(path) {
    const url = testRunner.url(path);
    const requestSentPromise = dp.Network.onceRequestWillBeSent(e => e.params.request.url === url);
    dp.Runtime.evaluate({expression: `fetch('${url}').catch(e => {})`});
    const {params: {requestId}} = await requestSentPromise;
    const event = await Promise.race([
      dp.Network.onceLoadingFailed(e => e.params.requestId === requestId),
      dp.Network.onceResponseReceived(e => e.params.requestId === requestId),
    ]);
    return event.method === 'Network.loadingFailed' && event.params.blockedReason === 'inspector';
  }

  /**
   * @param {string} path
   * @param {boolean} shouldBeBlocked
   */
  async function testRequest(path, shouldBeBlocked) {
    const blocked = await isBlocked(path);
    if (blocked === shouldBeBlocked) {
      testRunner.log(`Request for ${path} was ${blocked ? '' : 'not '}blocked as expected.`);
    } else {
      testRunner.log(`ERROR: Request for ${path} was ${blocked ? '' : 'not '}blocked but should ${shouldBeBlocked ? '' : 'not '}have been.`);
    }
  }

  testRunner.log('Testing `urls` parameter:');
  await dp.Network.setBlockedURLs({urls: ['*a.html']});
  await testRequest('resources/a.html', true);
  await testRequest('resources/b.html', false);

  testRunner.log('\nTesting `urlPatterns` parameter:');
  await dp.Network.setBlockedURLs(
      {urlPatterns: [{urlPattern: 'http://*:*/*/b.html', block: true}]});
  await testRequest('resources/a.html', false);
  await testRequest('resources/b.html', true);

  testRunner.log('\nTesting `urls` and `urlPatterns` combined:');
  await dp.Network.setBlockedURLs({
    urls: ['*a.html'],
    urlPatterns: [{urlPattern: 'http://*:*/*/b.html', block: true}]
  });
  await testRequest('resources/a.html', true);
  await testRequest('resources/b.html', true);
  await testRequest('resources/c.html', false);

  testRunner.log('\nTesting allowlisting');
  await dp.Network.setBlockedURLs({
    urlPatterns: [
      {urlPattern: 'http://*:*/*/b.html', block: false},
      {urlPattern: 'http://*:*/*', block: true}
    ]
  });
  await testRequest('resources/a.html', true);
  await testRequest('resources/b.html', false);
  await testRequest('resources/c.html', true);

  testRunner.log('\nTesting whether blocking state survives reloads');
  await dp.Page.reload();
  await testRequest('resources/a.html', true);
  await testRequest('resources/b.html', false);
  await testRequest('resources/c.html', true);


  testRunner.log('\nTesting clearing blocked URLs:');
  await dp.Network.setBlockedURLs({urls: [], urlPatterns: []});
  await testRequest('resources/a.html', false);
  await testRequest('resources/b.html', false);

  testRunner.log('\nTest setting patterns that fail to parse:');
  testRunner.log(await dp.Network.setBlockedURLs({urlPatterns: [{urlPattern: 'ht tp://', block: true}]}));
  testRunner.log(await dp.Network.setBlockedURLs({urlPatterns: [{urlPattern: '*.css', block: true}]}));

  testRunner.completeTest();
})
