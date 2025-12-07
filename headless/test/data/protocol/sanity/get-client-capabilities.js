// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests PublicKeyCredential.getClientCapabilities() in a secure context.`);

  const FetchHelper =
      await testRunner.loadScriptAbsolute('../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  await helper.enable();
  helper.onceRequest('https://test.com/index.html')
      .fulfill(FetchHelper.makeContentResponse('<html></html>'));

  await dp.Page.navigate({url: 'https://test.com/index.html'});

  const result = await session.evaluateAsync(
      `PublicKeyCredential.getClientCapabilities()`);

  testRunner.completeTest();
})
