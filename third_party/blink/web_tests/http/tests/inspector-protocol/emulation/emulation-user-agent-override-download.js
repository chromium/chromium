(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that Emulation.setUserAgentOverride applies to renderer-initiated downloads.`);

  await dp.Fetch.enable();
  await dp.Emulation.setUserAgentOverride({userAgent: 'Test UA'});

  session.evaluateAsyncWithUserGesture(`
    const a = document.createElement('a');
    a.href = '/inspector-protocol/emulation/resources/echo-headers.php';
    a.setAttribute('download', 'filename.txt');
    a.textContent = 'Download me';
    document.body.appendChild(a);
    a.click();
    undefined;
  `);
  testRunner.log('Clicked the link');

  const interceptedEvent = await dp.Fetch.onceRequestPaused();
  testRunner.log('Intercepted the download');

  const request = interceptedEvent.params.request;
  testRunner.log(`url: ${request.url}`);
  testRunner.log(`user agent: ${request.headers['User-Agent']}`);
  testRunner.completeTest();
})
