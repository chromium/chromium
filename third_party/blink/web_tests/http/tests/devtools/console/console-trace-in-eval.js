// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that when console.trace is called in eval'ed script ending with //# sourceURL=url it will dump a stack trace that will have the url as the script source. Bug 47252.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      function evalSource()
      {
          function b()
          {
              console.trace();
          }

          function a()
          {
              b();
          }

          a();
      }

      function doEvalSource()
      {
          setTimeout(function() {
              eval("(" + evalSource + ")()//# sourceURL=evalURL.js");
              //# sourceURL=console-trace-in-eval.js
          }, 0);
      }
  `);

  async function callback() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
  TestRunner.evaluateInPage('doEvalSource()');
  ConsoleTestRunner.addConsoleSniffer(callback);
})();
