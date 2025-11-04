(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} =
      await testRunner.startBlank('Tests emulation of the user agent.');

  // User Agent
  testRunner.log('\nChecking page navigator.userAgent');
  testRunner.log('...overriding userAgent to "Test UA"');
  await dp.Emulation.setUserAgentOverride({userAgent: 'Test UA'});
  testRunner.log(
      'navigator.userAgent == ' +
      await session.evaluate('navigator.userAgent'));
  await printHeader('User-Agent');
  // Change User Agent again
  testRunner.log('...overriding userAgent again to "Another Test UA"');
  await dp.Emulation.setUserAgentOverride({userAgent: 'Another Test UA'});
  testRunner.log(
      'navigator.userAgent == ' +
      await session.evaluate('navigator.userAgent'));
  await printHeader('User-Agent');

  const some_accept_language = 'ko, en, zh-CN, zh-HK, en-US, en-GB';
  const another_accept_language = 'en, zh-CN, zh-HK, en-US, en-GB';

  // Accept Language
  testRunner.log('\nChecking page navigator.language');
  testRunner.log(`...overriding acceptLanguage to "${some_accept_language}"`);
  await dp.Emulation.setUserAgentOverride(
      {userAgent: '', acceptLanguage: some_accept_language});
  testRunner.log(
      'navigator.language == ' + await session.evaluate('navigator.language'));
  testRunner.log(
      'navigator.languages == ' +
      await session.evaluate('navigator.languages'));
  // Change Accept Language again
  testRunner.log(
      `...overriding acceptLanguage to "${another_accept_language}"`);
  await dp.Emulation.setUserAgentOverride(
      {userAgent: '', acceptLanguage: another_accept_language});
  testRunner.log(
      'navigator.language == ' + await session.evaluate('navigator.language'));
  testRunner.log(
      'navigator.languages == ' +
      await session.evaluate('navigator.languages'));

  // Worker Accept Language
  testRunner.log('\nChecking worker navigator.language');
  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  const attachedPromise = dp.Target.onceAttachedToTarget();
  await session.evaluate(`var w = new Worker('${
      testRunner.url('resources/language-worker.js')}')`);
  const event = await attachedPromise;
  const workerSession = session.createChild(event.params.sessionId);
  testRunner.log(`...overriding acceptLanguage to "${some_accept_language}"`);
  await workerSession.protocol.Emulation.setUserAgentOverride(
      {userAgent: '', acceptLanguage: some_accept_language});

  testRunner.log('workerNavigator.language == ' + await session.evaluateAsync(`
      w.postMessage('ping!');
      new Promise(resolve => w.onmessage = e => resolve(e.data.language))`));
  testRunner.log('workerNavigator.languages == ' + await session.evaluateAsync(`
      w.postMessage('ping!');
      new Promise(resolve => w.onmessage = e => resolve(e.data.languages))`));

  testRunner.log(
      `...overriding acceptLanguage to "${another_accept_language}"`);
  await workerSession.protocol.Emulation.setUserAgentOverride(
      {userAgent: '', acceptLanguage: another_accept_language});

  testRunner.log('workerNavigator.language == ' + await session.evaluateAsync(`
      w.postMessage('ping!');
      new Promise(resolve => w.onmessage = e => resolve(e.data.language))`));
  testRunner.log('workerNavigator.languages == ' + await session.evaluateAsync(`
      w.postMessage('ping!');
      new Promise(resolve => w.onmessage = e => resolve(e.data.languages))`));

  // Close the worker.
  const detachedPromise = dp.Target.onceDetachedFromTarget();
  void session.evaluateAsync(`w.postMessage('close');`);
  await detachedPromise;

  testRunner.log('\nChecking Accept-Language header');
  testRunner.log(`...overriding acceptLanguage to "${some_accept_language}"`);
  await workerSession.protocol.Emulation.setUserAgentOverride(
      {userAgent: '', acceptLanguage: some_accept_language});
  await printHeader('Accept-Language');
  // Do not override explicit Accept-Language header.
  await printHeaderWithLang('Accept-Language');

  testRunner.log(
      `...overriding acceptLanguage to "${another_accept_language}"`);
  await workerSession.protocol.Emulation.setUserAgentOverride(
      {userAgent: '', acceptLanguage: 'zh-HK, en-US, en-GB'});
  await printHeader('Accept-Language');
  // Do not override explicit Accept-Language header.
  await printHeaderWithLang('Accept-Language');

  // Platform
  testRunner.log('\nChecking page navigator.platform');
  testRunner.log('...setting platform to "new_platform"');
  await dp.Emulation.setUserAgentOverride(
      {userAgent: '', platform: 'new_platform'});
  testRunner.log(
      'navigator.platform == ' + await session.evaluate('navigator.platform'));
  // Change Platform again
  testRunner.log('...setting platform again to "another_platform"');
  await dp.Emulation.setUserAgentOverride(
      {userAgent: '', platform: 'another_platform'});
  testRunner.log(
      'navigator.platform == ' + await session.evaluate('navigator.platform'));

  async function printHeader(name) {
    const url = testRunner.url('resources/echo-headers.php');
    const headers =
        await session.evaluateAsync(`fetch("${url}").then(r => r.text())`);
    for (const header of headers.split('\n')) {
      if (header.startsWith(name))
        testRunner.log(`Header ${header}`);
    }
  }

  async function printHeaderWithLang(name) {
    const url = testRunner.url('resources/echo-headers.php');
    const headers = await session.evaluateAsync(`fetch("${
        url}", { headers: {"accept-language": "zh-CN"}}).then(r => r.text())`);
    for (const header of headers.split('\n')) {
      if (header.toLowerCase().startsWith(name.toLowerCase()))
        testRunner.log(`Fetch with custom header ${header}`);
    }
  }

  testRunner.completeTest();
})
