// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(
      `Tests that when uncaught exception in eval'ed script ending with //# sourceURL=url is logged into console, its stack trace will have the url as the script source. Bug 47252.\n`);
  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
      function evalSource(name)
      {
          function b()
          {
              throw new Error("Exception in eval:" + name);
          }

          function a()
          {
              b();
          }

          a();
      }

      function doEvalWithSourceURL()
      {
          var source = "(" + evalSource + ")(\\"with sourceURL\\")//# sourceURL=evalURL.js";
          setTimeout(eval.bind(this, source), 0);
      }

      function doAnonymousEvalWith()
      {
          var source = "(" + evalSource + ")(\\"anonymous\\")";
          setTimeout(eval.bind(this, source), 0);
      }
  `);

  TestRunner.evaluateInPage('doEvalWithSourceURL()', step2.bind(this));

  function step2() {
    TestRunner.evaluateInPage('doAnonymousEvalWith()', step3.bind(this));
  }

  function step3() {
    if (Console.ConsoleView.ConsoleView.instance().visibleViewMessages.length < 2)
      ConsoleTestRunner.addConsoleSniffer(step3);
    else
      step4();
  }

  function step4() {
    ConsoleTestRunner.expandConsoleMessages(onExpanded);
  }

  async function onExpanded() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
