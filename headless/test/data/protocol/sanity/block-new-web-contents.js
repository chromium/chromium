// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --block-new-web-contents
// META: --disable-popup-blocking

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that --block-new-web-contents blocks new web contents creation.');

  const {sessionId} =
      (await testRunner.browserP().Target.attachToBrowserTarget({})).result;
  const bp = (testRunner.createSessionFor(sessionId)).protocol;

  const HttpInterceptor =
      await testRunner.loadScriptAbsolute('../resources/http-interceptor.js');
  const httpInterceptor = await (new HttpInterceptor(testRunner, bp)).init();
  httpInterceptor.setDisableRequestedUrlsLogging(true);

  httpInterceptor.addResponse('https://example.com/index.html', `
      <html>
      <body style="margin: 0; padding: 0;">
        <a href="https://example.com/page2.html" target="_blank"
           style="display: block; width: 100px; height: 100px;">Click</a>
      </body>
      </html>
  `);
  httpInterceptor.addResponse('https://example.com/page2.html', `
      <html><body>Page 2</body></html>
  `);

  await session.navigate('https://example.com/index.html');

  async function getPageTargetsCount() {
    const targetsResponse = await dp.Target.getTargets();
    const pageTargets =
        targetsResponse.result.targetInfos.filter(t => t.type === 'page');
    return pageTargets.length;
  }

  const pageTargetsCount = await getPageTargetsCount();

  const winOpenResult =
      await session.evaluate('window.open(\'https://example.com/page2.html\')');

  const newPageTargetsCount = (await getPageTargetsCount()) - pageTargetsCount;

  testRunner.log(`window.open() result: ${winOpenResult}`);
  testRunner.log(`New page targets count: ${newPageTargetsCount}`);

  testRunner.completeTest();
});
