// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that uncaught exceptions are logged into console.Bug 47250.\n`);
  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
      function loadIframe()
      {
          var iframe = document.createElement("iframe");
          iframe.src = "resources/uncaught-in-iframe.html";
          document.body.appendChild(iframe);
      }
  `);

  ConsoleTestRunner.addConsoleViewSniffer(addMessage, true);
  TestRunner.evaluateInPage('loadIframe()');
  function addMessage(viewMessage) {
    if (viewMessage.element().deepTextContent().indexOf('setTimeout') !== -1)
      ConsoleTestRunner.expandConsoleMessages(onExpanded);
  }

  async function onExpanded() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
