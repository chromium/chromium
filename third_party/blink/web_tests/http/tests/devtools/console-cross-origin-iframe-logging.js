// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that cross origin errors are logged with source url and line number.\n`);
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.navigatePromise("http://example.test:8000/devtools/resources/empty.html");
  // NOTE: evaluateInPageAsync() waits on the promise at the end of block before
  // resolving the promise it returned. Other forms of the evaluate including
  // evaluateInPagePromise() do not do this.
  await TestRunner.evaluateInPageAsync(`
    const frame = document.createElement('iframe');
    frame.src = 'http://other.origin.example.test:8000/devtools/resources/cross-origin-iframe.html';
    document.body.appendChild(frame);
    new Promise(f => frame.onload = f);
  `);

  ConsoleTestRunner.addConsoleSniffer(finish);
  Common.settingForTest('monitoringXHREnabled').set(true);
  await TestRunner.evaluateInPagePromise(`
    // Should fail.
    try {
      var host = frames[0].location.host;
    } catch (e) {}

    // Should fail.
    try {
      frames[0].location.reload();
    } catch (e) {}

    // Should fail.
    frames[0].postMessage("fail", "http://example.test:8000");
  `);

  async function finish() {
    Common.settingForTest('monitoringXHREnabled').set(false);
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
