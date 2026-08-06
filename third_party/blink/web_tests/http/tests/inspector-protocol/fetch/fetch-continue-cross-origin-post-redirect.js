// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that Fetch.continueRequest can continue a POST navigation that is ` +
      `redirected cross-origin.\n`);

  // On a cross-origin redirect the browser recomputes the Origin header to the
  // serialized opaque origin ("null"), mirroring what the network service does.
  // That value used to be reported back as a client header modification, which
  // the network service rejects, failing the navigation with
  // net::ERR_INVALID_ARGUMENT even though the client never touched Origin.
  const echoUrl = 'http://localhost:8000/inspector-protocol/network/resources/' +
      'echo-headers.php?headers=REQUEST_METHOD:HTTP_ORIGIN';
  const redirectUrl =
      'http://127.0.0.1:8000/inspector-protocol/fetch/resources/' +
      `redirect-307.php?${echoUrl}`;

  await dp.Fetch.enable();
  await dp.Network.enable();

  // Continue every request unmodified, including the cross-origin redirect.
  dp.Fetch.onRequestPaused(event => {
    testRunner.log(`Continuing ${event.params.request.method} ${
        event.params.request.url}`);
    dp.Fetch.continueRequest({requestId: event.params.requestId});
  });

  // Submit a top-level POST navigation that gets redirected cross-origin.
  session.evaluate(`
    const form = document.createElement('form');
    form.method = 'POST';
    form.action = '${redirectUrl}';
    document.body.appendChild(form);
    form.submit();
  `);

  await dp.Page.enable();
  await dp.Page.onceLoadEventFired();

  // The 307 preserves the method, and the destination sees the opaque origin
  // that the network service computes for it.
  testRunner.log('\nDestination saw:');
  testRunner.log((await session.evaluate('document.body.innerText')).trim());

  testRunner.completeTest();
})
