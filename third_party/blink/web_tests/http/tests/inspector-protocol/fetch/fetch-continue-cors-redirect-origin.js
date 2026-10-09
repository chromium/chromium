// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that Fetch.continueRequest keeps the Origin header on a ` +
      `same-origin fetch that is redirected cross-origin.`);

  // The request starts same-origin, so it has no Origin header. On the
  // cross-origin redirect CorsURLLoader adds one, which used to be dropped
  // along with the Origin the interceptor recomputes for the redirect, so the
  // destination saw no Origin at all.
  const echoUrl = 'http://localhost:8000/inspector-protocol/fetch/resources/' +
      'cors-echo-headers.php?headers=HTTP_ORIGIN:HTTP_X_DEVTOOLS_TEST';
  const redirectUrl = 'http://127.0.0.1:8000/inspector-protocol/fetch/' +
      `resources/redirect.pl?${echoUrl}`;

  await dp.Fetch.enable();

  // Continue every request, overriding the headers of the cross-origin
  // redirect with `redirectHeaders` when it is set.
  let redirectHeaders;
  dp.Fetch.onRequestPaused(event => {
    const {requestId, request} = event.params;
    testRunner.log(`Continuing ${request.method} ${request.url}`);
    if (redirectHeaders && request.url === echoUrl) {
      dp.Fetch.continueRequest({requestId, headers: redirectHeaders});
    } else {
      dp.Fetch.continueRequest({requestId});
    }
  });

  // Fetch a same-origin URL that redirects cross-origin.
  async function fetchAndLog(description) {
    testRunner.log(`\n${description}`);
    const body = await session.evaluateAsync(
        `fetch('${redirectUrl}').then(r => r.text())`);
    testRunner.log('Destination saw:');
    testRunner.log(body.trim());
  }

  await fetchAndLog('Redirect continued unmodified:');

  redirectHeaders = [{name: 'X-DevTools-Test', value: 'added'}];
  await fetchAndLog('Redirect continued with a header added:');

  redirectHeaders = [{name: 'Origin', value: 'http://example.test'}];
  await fetchAndLog('Redirect continued with Origin overridden:');

  testRunner.completeTest();
})
