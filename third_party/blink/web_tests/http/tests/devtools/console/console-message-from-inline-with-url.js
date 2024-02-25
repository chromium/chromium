// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that log message and syntax errors from inline scripts with sourceURL are logged into console, contains correct link and doesn't cause browser crash.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      function foo()
      {
          console.log("foo");
      }
      function addInlineWithSyntaxError()
      {
          var headElement = document.getElementsByTagName("head")[0];
          var scriptElement = document.createElement("script");
          scriptElement.setAttribute("type", "text/javascript");
          headElement.appendChild(scriptElement);
          scriptElement.text = "}\\n//# sourceURL=boo.js";
      }
      //# sourceURL=foo.js
    `);

  TestRunner.evaluateInPage('setTimeout(foo, 0);', ConsoleTestRunner.waitUntilMessageReceived.bind(this, step2));

  function step2() {
    TestRunner.evaluateInPage(
        'setTimeout(addInlineWithSyntaxError, 0);', ConsoleTestRunner.waitUntilMessageReceived.bind(this, step3));
  }

  async function step3() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
