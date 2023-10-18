// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests asynchronous call stacks printed in console for a Network.Initiator.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          debugger;
          console.clear();
          setTimeout(timeout1, 0);
      }

      function timeout1()
      {
          setTimeout(timeout2, 0);
      }

      function timeout2()
      {
          sendXHR();
      }

      function sendXHR()
      {
          var xhr = new XMLHttpRequest();
          xhr.open("POST", "/failure/foo", true /* async */);
          xhr.send();
      }
  `);

  var maxAsyncCallStackDepth = 8;
  var numberOfConsoleMessages = 2;

  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(SDK.DebuggerModel.PauseOnExceptionsState.DontPauseOnExceptions);
    TestRunner.DebuggerAgent.setAsyncCallStackDepth(0);
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  async function step2() {
    await TestRunner.DebuggerAgent.setAsyncCallStackDepth(maxAsyncCallStackDepth);
    ConsoleTestRunner.waitUntilNthMessageReceived(numberOfConsoleMessages, expandAndDumpConsoleMessages);
    SourcesTestRunner.resumeExecution();
  }

  function expandAndDumpConsoleMessages() {
    ConsoleTestRunner.expandConsoleMessages(() => {
      // This is to handle asynchronous console message rendering through runtime extensions.
      setTimeout(dumpConsoleMessages, 0);
    });
  }

  async function dumpConsoleMessages() {
    await ConsoleTestRunner.dumpConsoleMessages(false, false, TestRunner.textContentWithLineBreaksTrimmed);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
