// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests debug(fn) console command.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function simpleTestFunction()
      {
         return 0;
      }
    `);
  await TestRunner.evaluateInPagePromise(`
      function simpleTestFunction1() { return 0; } function simpleTestFunction2() { return 0; }
    `);
  await TestRunner.evaluateInPagePromise(`
      function simpleTestFunction3() { Math.random(); debugger; }
    `);

  var currentSourceFrame;
  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.runDebuggerTestSuite([
    function testSetSimpleBreakpoint(next) {
      setBreakpointAndRun(next, 'simpleTestFunction', 'simpleTestFunction();');
    },

    function testSetBreakpointOnFirstFunctionInLine(next) {
      setBreakpointAndRun(next, 'simpleTestFunction1', 'simpleTestFunction2(); simpleTestFunction1();');
    },

    function testSetBreakpointOnLastFunctionInLine(next) {
      setBreakpointAndRun(next, 'simpleTestFunction2', 'simpleTestFunction1(); simpleTestFunction2();');
    },

    function testRemoveBreakpoint(next) {
      ConsoleTestRunner.evaluateInConsole('debug(simpleTestFunction3); undebug(simpleTestFunction3);');
      ConsoleTestRunner.evaluateInConsole('setTimeout(simpleTestFunction3, 0)');
      SourcesTestRunner.waitUntilPaused(didPause1);

      function didPause1(callFrames, reason) {
        TestRunner.addResult('Script execution paused.');
        TestRunner.addResult(
            'Reason for pause: ' +
            (reason ==  Protocol.Debugger.PausedEventReason.DebugCommand ? 'debug command' : 'debugger statement') + '.');
        next();
      }
    }
  ]);

  async function setBreakpointAndRun(next, functionName, runCmd) {
    await ConsoleTestRunner.evaluateInConsolePromise('debug(' + functionName + ')');

    TestRunner.addResult('Breakpoint added.');
    await ConsoleTestRunner.evaluateInConsolePromise('setTimeout(function() { ' + runCmd + ' }, 20)');
    TestRunner.addResult('Set timer for test function.');
    SourcesTestRunner.waitUntilPaused(didPause);

    async function didPause(callFrames, reason) {
      TestRunner.addResult('Script execution paused.');
      await SourcesTestRunner.captureStackTrace(callFrames);
      await ConsoleTestRunner.evaluateInConsolePromise('undebug(' + functionName + ')');
      TestRunner.addResult('Breakpoint removed.');
      TestRunner.assertEquals(reason,  Protocol.Debugger.PausedEventReason.DebugCommand);
      SourcesTestRunner.resumeExecution(didResume);
    }

    function didResume() {
      TestRunner.addResult('Script execution resumed.');
      next();
    }
  }
})();
