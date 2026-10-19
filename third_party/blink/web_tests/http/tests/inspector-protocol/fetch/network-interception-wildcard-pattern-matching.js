// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that Fetch.enable URL patterns support '*' and '?' wildcards.\n`);

  const urlPrefix = testRunner.url('').replace(/\/$/, '');
  const resourceURL = urlPrefix + '/bar.js';

  // Mask the test directory so that the output doesn't depend on it.
  function cleanURLOrPattern(urlOrPattern) {
    if (urlOrPattern.startsWith(urlPrefix)) {
      return '(MASKED_URL_PATH)' + urlOrPattern.substr(urlPrefix.length);
    }
    return urlOrPattern;
  }

  dp.Fetch.onRequestPaused(({params}) => {
    testRunner.log('Intercepted Request: ' +
                   cleanURLOrPattern(params.request.url));
    dp.Fetch.continueRequest({requestId: params.requestId});
  });

  async function checkPattern(pattern) {
    testRunner.log('Setting Pattern: ' + cleanURLOrPattern(pattern));
    await dp.Fetch.enable({patterns: [{urlPattern: pattern}]});
    testRunner.log('Requesting: ' + cleanURLOrPattern(resourceURL));
    await session.evaluateAsync(url => fetch(url).then(() => {}), resourceURL);
    testRunner.log('Response Received: ' + cleanURLOrPattern(resourceURL));
    await dp.Fetch.disable();
    testRunner.log('');
  }

  await checkPattern('**bar.js');
  await checkPattern(resourceURL);
  await checkPattern('*bar.js');
  await checkPattern('bar.js');
  await checkPattern('\\*bar.js');
  await checkPattern('*b*');
  await checkPattern('*');
  await checkPattern('*bar_js');
  await checkPattern('*bar?js');

  testRunner.completeTest();
});
