// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that pausing on uncaught exceptions thrown from C++ bindings will not crash.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      var functions;

      function testFunction()
      {
          console.clear();
          // This used to be a racy crash. Test some sequence of functions.
          functions = [f2, f1, f2, f1, f2, f1, f2, f1];
          functions.push(function() {});
          functions.shift()();
      }

      function f1() {
          setTimeout(functions.shift(), 0);
          document.body.appendChild("<throw_exception>");
      }

      function f2() {
          setTimeout(functions.shift(), 0);
          new Range().compareBoundaryPoints(1, 2);
      }
  `);

  var expectedErrorsCount = 8;

  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(SDK.DebuggerModel.PauseOnExceptionsState.PauseOnUncaughtExceptions);
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(didPause);
  }

  function didPause(callFrames, reason, breakpointIds, asyncStackTrace) {
    --expectedErrorsCount;
    if (!expectedErrorsCount) {
      ConsoleTestRunner.waitUntilNthMessageReceived(1, step2);
      SourcesTestRunner.resumeExecution();
    } else {
      SourcesTestRunner.resumeExecution(SourcesTestRunner.waitUntilPaused.bind(SourcesTestRunner, didPause));
    }
  }

  async function step2() {
    await ConsoleTestRunner.dumpConsoleMessages();
    completeTest();
  }

  function completeTest() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(SDK.DebuggerModel.PauseOnExceptionsState.DontPauseOnExceptions);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
