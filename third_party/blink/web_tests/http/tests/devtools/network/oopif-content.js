// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  // TestRunner.startDumpingProtocolMessages();
  TestRunner.addResult(`Tests content is available for a cross-process iframe.\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');
  NetworkTestRunner.recordNetwork();
  await TestRunner.addIframe('http://devtools.oopif.test:8000/devtools/resources/cross-origin-iframe.html');
  const request = NetworkTestRunner.networkRequests().pop();
  if (!request || !request.finished) {
    TestRunner.addResult('FAILED: request not finished!');
    TestRunner.completeTest();
    return;
  }
  const { content, error, isEncoded } = await request.requestContent();
  TestRunner.addResult(`content: ${content}`);
  TestRunner.completeTest();
})();
