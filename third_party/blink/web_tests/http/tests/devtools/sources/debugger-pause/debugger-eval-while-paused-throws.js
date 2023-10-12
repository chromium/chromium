// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that evaluation in console that throws works fine when script is paused.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      var globalObj = {
          func: function()
          {
              throw new Error("globalObj.func");
          }
      };

      function testFunction()
      {
          var localObj = {
              func: function()
              {
                  throw new Error("localObj.func");
              }
          };
          debugger;
      }
      //# sourceURL=test.js
  `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function injectedFunction() {
    var injectedObj = {
      func: function() {
        throw new Error('injectedObj.func');
      }
    };
    return injectedObj.func();
  }

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2() {
    TestRunner.evaluateInPage(String(injectedFunction), step3);
  }

  function step3() {
    ConsoleTestRunner.evaluateInConsole('injectedFunction()', step4);
  }

  function step4() {
    ConsoleTestRunner.evaluateInConsole('localObj.func()', step5);
  }

  function step5() {
    ConsoleTestRunner.evaluateInConsole(
        'globalObj.func()', dumpConsoleMessages);
  }

  function dumpConsoleMessages() {
    TestRunner.deprecatedRunAfterPendingDispatches(async () => {
      TestRunner.addResult('Dumping console messages:\n');
      await ConsoleTestRunner.dumpConsoleMessages();
      SourcesTestRunner.completeDebuggerTest();
    });
  }
})();
