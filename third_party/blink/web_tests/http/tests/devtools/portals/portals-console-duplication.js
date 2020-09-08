// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console messages aren't duplicated after portal activation`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.navigatePromise('resources/console-duplication-host.html');
  const targets = SDK.targetManager.targets();
  TestRunner.assertEquals(3, targets.length);
  await ConsoleTestRunner.waitForConsoleMessagesPromise(3);
  await ConsoleTestRunner.dumpConsoleMessages();

  function activate() {
    TestRunner.evaluateInPage(`setTimeout(() => document.querySelector('portal').activate());`);
  }

  TestRunner.addResult('\nfirst activate (without adoption)');
  activate();
  await ConsoleTestRunner.waitUntilNthMessageReceivedPromise(2);
  TestRunner.assertEquals(3, ConsoleTestRunner.consoleMessagesCount());
  await ConsoleTestRunner.dumpConsoleMessages();

  TestRunner.addResult('\nsecond activate (with adoption)');
  activate();
  await ConsoleTestRunner.waitUntilNthMessageReceivedPromise(2);
  TestRunner.assertEquals(3, ConsoleTestRunner.consoleMessagesCount());
  await ConsoleTestRunner.dumpConsoleMessages();

  TestRunner.completeTest();
})();
