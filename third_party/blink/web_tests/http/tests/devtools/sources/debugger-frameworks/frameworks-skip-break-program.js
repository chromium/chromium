// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that framework blackboxing skips instant pauses (e.g. breakpoints on console.assert(), setTimeout(), etc.) if they happen entirely inside the framework.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <input type="button" onclick="testFunction()" value="Test">
    `);
  await TestRunner.addScriptTag('../debugger/resources/framework.js');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          debugger;
          Promise.resolve()
              // Should not pause on console.assert() inside framework.
              .then(Framework.bind(Framework.assert, null, false, "Assert from test"))
              // Should not pause on setTimeout inside the framework.
              .then(Framework.willSchedule(Framework.empty, 0))
              .then(stop)
              .catch(function FAIL_should_not_pause(e) { debugger; throw e; });
      }

      function stop()
      {
          setTimeout(function() {}, 0); // Should pause here.
      }
  `);

  var frameworkRegexString = '/framework\\.js$';
  Common.Settings.settingForTest('skipStackFramesPattern').set(frameworkRegexString);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(SDK.DebuggerModel.PauseOnExceptionsState.PauseOnAllExceptions);
    SourcesTestRunner.setEventListenerBreakpoint('instrumentation:setTimeout', true);
    SourcesTestRunner.resumeExecution(SourcesTestRunner.waitUntilPaused.bind(SourcesTestRunner, didPause));
  }

  async function didPause(callFrames, reason, breakpointIds, asyncStackTrace) {
    await SourcesTestRunner.captureStackTrace(callFrames);
    completeTest();
  }

  function completeTest() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(SDK.DebuggerModel.PauseOnExceptionsState.DontPauseOnExceptions);
    SourcesTestRunner.setEventListenerBreakpoint('instrumentation:setTimeout', false);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
