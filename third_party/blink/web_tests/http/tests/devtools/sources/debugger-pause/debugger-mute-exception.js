// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that pause on exception is muted when conditional breakpoint is set to "false".`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function throwAnException()
      {
          var a;
          try {
              a.foo = 1; // Should not stop here.
          } catch (e) {
          }

          debugger; // Should not stop here.

          try {
              a.bar = 1; // Should stop here.
          } catch (e) {
          }
      }

      function handleClick()
      {
          throwAnException();
      }
      //# sourceURL=test.js
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(
        SDK.DebuggerModel.PauseOnExceptionsState.PauseOnAllExceptions);
    SourcesTestRunner.showScriptSource('test.js', step2);
  }

  async function step2(sourceFrame) {
    TestRunner.addResult('Script source was shown.');
    await SourcesTestRunner.setBreakpoint(sourceFrame, 10, 'false', true);
    await SourcesTestRunner.setBreakpoint(sourceFrame, 14, 'false', true);
    TestRunner.evaluateInPage('setTimeout(handleClick, 0)');
    SourcesTestRunner.waitUntilPausedAndDumpStackAndResume(step3);
  }

  function step3() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(
        SDK.DebuggerModel.PauseOnExceptionsState.DontPauseOnExceptions);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
