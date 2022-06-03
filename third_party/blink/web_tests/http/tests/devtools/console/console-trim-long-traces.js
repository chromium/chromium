// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that a long stack trace is truncated.\n`);

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    function recursive(n) {
      if (n > 1) {
        return recursive(n-1);
      } else {
        return console.trace();
      }
    }
    recursive(10);
    recursive(50);
  `);

  await ConsoleTestRunner.waitForConsoleMessagesPromise(2);
  await ConsoleTestRunner.dumpConsoleMessages(false, false, messageElement => '\n' + messageElement.deepTextContent().replace(/\u200b/g, ''));
  TestRunner.completeTest();
})();
