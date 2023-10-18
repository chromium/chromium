// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that pause on exception in internal script does not crash.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function throwException()
      {
          new Function("return ()");
      }

      function handleClick()
      {
          throwException();
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(
        SDK.DebuggerModel.PauseOnExceptionsState.PauseOnUncaughtExceptions);
    SourcesTestRunner.showScriptSource('debugger-pause-in-internal.js', step2);
  }

  function step2() {
    TestRunner.addResult('Script source was shown.');
    TestRunner.evaluateInPage('setTimeout(handleClick, 0)');
    SourcesTestRunner.waitUntilPaused(step3);
  }

  async function step3(callFrames) {
    await SourcesTestRunner.captureStackTrace(callFrames);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
