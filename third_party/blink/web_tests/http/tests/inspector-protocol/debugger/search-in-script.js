// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests script search in inspector debugger agent.\n`);

  await dp.Debugger.enable();

  const scriptsByUrl = new Map();
  dp.Debugger.onScriptParsed(event => {
    scriptsByUrl.set(event.params.url, event.params.scriptId);
  });

  const scriptParsedPromise = dp.Debugger.onceScriptParsed(
      event => event.params.url.endsWith('/resources/search.js'));

  await session.evaluateAsync(`
    new Promise(resolve => {
      const iframe = document.createElement('iframe');
      iframe.src = '${testRunner.url('resources/search.html')}';
      iframe.onload = () => resolve();
      document.body.appendChild(iframe);
    })
    //# sourceURL=driver.js
  `);

  const scriptParsed = await scriptParsedPromise;
  const scriptId = scriptParsed.params.scriptId;
  const driverScriptId = scriptsByUrl.get('driver.js');
  testRunner.log(scriptParsed.params.url);

  await dp.Debugger.getScriptSource({scriptId});

  function dumpSearchMatches(response) {
    testRunner.log('Search matches:');
    const matches = response.result.result;
    if (matches.length === 0) {
      testRunner.log('No search matches');
    } else {
      for (const match of matches) {
        testRunner.log(
            `lineNumber: ${match.lineNumber}, line: '${match.lineContent}'`);
      }
    }
    testRunner.log('');
  }

  // This file should not match search query.
  const text = 'searchTest' +
      'UniqueString';
  dumpSearchMatches(await dp.Debugger.searchInContent({
    scriptId,
    query: text,
    caseSensitive: false,
    isRegex: false,
  }));

  dumpSearchMatches(await dp.Debugger.searchInContent({
    scriptId,
    query: text,
    caseSensitive: true,
    isRegex: false,
  }));

  const regexText = '[a-z]earchTestUniqueString';
  dumpSearchMatches(await dp.Debugger.searchInContent({
    scriptId,
    query: regexText,
    caseSensitive: false,
    isRegex: true,
  }));

  dumpSearchMatches(await dp.Debugger.searchInContent({
    scriptId,
    query: regexText,
    caseSensitive: true,
    isRegex: true,
  }));

  dumpSearchMatches(await dp.Debugger.searchInContent({
    scriptId: driverScriptId,
    query: text,
    caseSensitive: false,
    isRegex: false,
  }));

  testRunner.completeTest();
});
