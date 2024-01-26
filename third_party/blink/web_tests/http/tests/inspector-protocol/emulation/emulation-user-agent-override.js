(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests emulation of the user agent.');

  // User Agent
  await dp.Emulation.setUserAgentOverride({userAgent: 'Test UA'});
  testRunner.log('navigator.userAgent == ' + await session.evaluate('navigator.userAgent'));
  await printHeader('User-Agent');

  // Accept Language
  await dp.Emulation.setUserAgentOverride({userAgent: '', acceptLanguage: 'ko, en, zh-CN, zh-HK, en-US, en-GB'});
  testRunner.log('navigator.language == ' + await session.evaluate('navigator.language'));

  // Worker Accept Language
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  const attachedPromise = dp.Target.onceAttachedToTarget();
  await session.evaluate(`var w = new Worker('${testRunner.url('resources/worker.js')}')`);
  const event = await attachedPromise;
  const workerSession = session.createChild(event.params.sessionId);
  await workerSession.protocol.Emulation.setUserAgentOverride({userAgent: '', acceptLanguage: 'ko, en, zh-CN, zh-HK, en-US, en-GB'});

  const detachedPromise = dp.Target.onceDetachedFromTarget()
  testRunner.log('workerNavigator.language == ' + await session.evaluateAsync(`
      w.postMessage('ping!');
      new Promise(resolve => w.onmessage = e => resolve(e.data.language))`));
  await detachedPromise;

  await printHeader('Accept-Language');

  // Do not override explicit Accept-Language header.
  await printHeaderWithLang('Accept-Language');

  // Platform
  await dp.Emulation.setUserAgentOverride({userAgent: '', platform: 'new_platform'});
  testRunner.log('navigator.platform == ' + await session.evaluate('navigator.platform'));

  async function printHeader(name) {
    const url = testRunner.url('resources/echo-headers.php');
    const headers = await session.evaluateAsync(`fetch("${url}").then(r => r.text())`);
    for (const header of headers.split('\n')) {
      if (header.startsWith(name))
        testRunner.log(header);
    }
  }

  async function printHeaderWithLang(name) {
    const url = testRunner.url('resources/echo-headers.php');
    const headers = await session.evaluateAsync(`fetch("${url}", { headers: {"accept-language": "ko"}}).then(r => r.text())`);
    for (const header of headers.split('\n')) {
      if (header.toLowerCase().startsWith(name.toLowerCase()))
        testRunner.log(header);
    }
  }

  testRunner.completeTest();
})
