// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that framework black-boxing skips exceptions, including those that happened deeper inside V8 native script.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <input type="button" onclick="testFunction()" value="Test">
    `);
  await TestRunner.addScriptTag('../debugger/resources/framework.js');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          Framework.throwFrameworkExceptionAndCatch();
          Framework.throwInNativeAndCatch();

          // All above should be skipped.
          debugger;
      }
  `);

  var frameworkRegexString = '/framework\\.js$';
  Common.Settings.settingForTest('skip-stack-frames-pattern').set(frameworkRegexString);

  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(SDK.DebuggerModel.PauseOnExceptionsState.PauseOnAllExceptions);
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(didPause);
  }

  async function didPause(callFrames, reason, breakpointIds, asyncStackTrace) {
    await SourcesTestRunner.captureStackTrace(callFrames);
    completeTest();
  }

  function completeTest() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(SDK.DebuggerModel.PauseOnExceptionsState.DontPauseOnExceptions);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
