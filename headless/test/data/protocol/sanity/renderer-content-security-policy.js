// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: content security policy.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  // Only first 3 scripts of 4 on the page are white listed for execution.
  // Therefore only 3 lines in the log are expected.
  httpInterceptor.addResponse(`http://example.com/`,
      `<!DOCTYPE html>` +
      `<script>console.log('pass256');</script>` +
      `<script>console.log('pass384');</script>` +
      `<script>console.log('pass512');</script>` +
      `<script>console.log('fail');</script>`,
      [`HTTP/1.1 200 OK`,
       `Content-Type: text/html`,
       `Content-Security-Policy: script-src` +
       ` 'sha256-INSsCHXoo4K3+jDRF8FSvl13GP22I9vcqcJjkq35Y20='` +
       ` 'sha384-77lSn5Q6V979pJ8W2TXc6Lrj98LughR0ofkFwa+qOEtlcofEdLPkOPtp` +
       `JF8QQMev'` +
       ` 'sha512-2cS3KZwfnxFo6lvBvAl113f5N3QCRgtRJBbtFaQHKOhk36sdYYKFvhCq` +
       `GTvbN7pBKUfsjfCQgFF4MSbCQuvT8A=='`,
      ]);

  // Regenerate sha256 hash with:
  // echo -n "console.log('pass256');" \
  //   | openssl sha256 -binary \
  //   | openssl base64

  await virtualTimeController.initialize(1000);
  await frameNavigationHelper.navigate('http://example.com/');
  await virtualTimeController.grantTime(500);
  testRunner.completeTest();
})
