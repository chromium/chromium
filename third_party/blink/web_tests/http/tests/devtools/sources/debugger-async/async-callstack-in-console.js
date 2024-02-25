// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests asynchronous call stacks printed in console.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <div><iframe src="../debugger/resources/post-message-listener.html" id="iframe" width="800" height="100" style="border: 1px solid black;">
      </iframe></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          debugger;
          console.clear();
          setTimeout(timeout1, 0);
      }

      function timeout1()
      {
          console.trace();
          setTimeout(timeout2, 0);
      }

      function timeout2()
      {
          setTimeout(timeout3, 0);
          throw Error("foo");
      }

      function timeout3()
      {
          console.trace();
          var iframeWidnow = document.getElementById("iframe").contentWindow;
          tryPostMessage(iframeWidnow, "http://www.example.com");
      }

      function tryPostMessage(win, origin)
      {
          try {
              win.postMessage("Trying origin=" + origin, origin);
          } catch(ex) {
              console.error("FAIL: Error sending message to " + origin + ". " + ex);
          }
      }
  `);

  var maxAsyncCallStackDepth = 8;
  var numberOfConsoleMessages = 5;

  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(SDK.DebuggerModel.PauseOnExceptionsState.DontPauseOnExceptions);
    TestRunner.DebuggerAgent.setAsyncCallStackDepth(0);
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2() {
    TestRunner.DebuggerAgent.setAsyncCallStackDepth(maxAsyncCallStackDepth).then(didPause);
  }

  function didPause() {
    ConsoleTestRunner.waitUntilNthMessageReceived(numberOfConsoleMessages, expandAndDumpConsoleMessages);
    SourcesTestRunner.resumeExecution();
  }

  function expandAndDumpConsoleMessages() {
    ConsoleTestRunner.expandConsoleMessages(dumpConsoleMessages);
  }

  async function dumpConsoleMessages() {
    await ConsoleTestRunner.dumpConsoleMessages(false, false, TestRunner.textContentWithLineBreaks);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
