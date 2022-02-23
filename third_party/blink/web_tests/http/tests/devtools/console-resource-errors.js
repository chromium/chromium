// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that errors to load a resource cause error messages to be logged to console.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var image = document.createElement('img');
          image.onerror = step2;
          image.src = "unknown-scheme://foo";
      }

      function step2() {
        loadXHR();
        loadIframe();
      }

      function loadXHR()
      {
          var xhr = new XMLHttpRequest();
          xhr.open("GET","non-existent-xhr", false);
          xhr.send(null);
      }

      function loadIframe()
      {
          var iframe = document.createElement("iframe");
          iframe.src = "resources/console-resource-errors-iframe.html";
          document.body.appendChild(iframe);
      }
  `);

  TestRunner.evaluateInPage('performActions()');
  await ConsoleTestRunner.waitForConsoleMessagesPromise(5);
  ConsoleTestRunner.expandConsoleMessages(onExpandedMessages);

  async function onExpandedMessages() {
    await ConsoleTestRunner.dumpConsoleMessagesWithClasses(true, true);
    TestRunner.completeTest();
  }
})();
