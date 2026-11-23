// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'resources/long-script-page.html',
      'Tests long script content is correctly shown in source panel after page reload.\n');

  await dp.Page.enable();
  await dp.Runtime.enable();
  await dp.Debugger.enable();

  const loadPromise = dp.Page.onceLoadEventFired();
  await dp.Page.reload({ignoreCache: true});
  await loadPromise;
  testRunner.log('Page reloaded.');

  const scriptParsedPromise = dp.Debugger.onceScriptParsed(
      event => event.params.url.endsWith('/long_script.cgi'));
  const consolePromise = dp.Runtime.onceConsoleAPICalled(
      event => event.params.args[0]?.value === 'finished');

  await session.evaluate('loadScript()');
  const [scriptParsed] =
      await Promise.all([scriptParsedPromise, consolePromise]);

  await session.evaluate('unloadScript()');
  await dp.HeapProfiler.collectGarbage();

  const response = await dp.Debugger.getScriptSource(
      {scriptId: scriptParsed.params.scriptId});
  if (response.error) {
    testRunner.fail(
        `Debugger.getScriptSource failed: ${response.error.message}`);
    return;
  }

  let loadedScript = response.result?.scriptSource;
  const expected = 'console.log(\'finished\');\n';
  if (!loadedScript) {
    testRunner.fail('No script content');
    return;
  }
  // On Windows we receive an additional \r at line ends.
  loadedScript = loadedScript.replace(/\r\n/g, '\n');
  const expectedLength = 1024 * 10240 + expected.length;
  if (loadedScript.length !== expectedLength) {
    testRunner.fail(`Loaded script length mismatch: expected ${
        expectedLength}, got ${loadedScript.length}`);
    return;
  }
  const actual = loadedScript.substring(loadedScript.length - expected.length);
  if (actual !== expected) {
    testRunner.fail(`Loaded script is corrupted: expected ${
        JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
    return;
  }

  testRunner.log('Test passed');
  testRunner.completeTest();
})
