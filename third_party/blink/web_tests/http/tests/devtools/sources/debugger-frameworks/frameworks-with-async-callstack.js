// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests the async call stacks and framework black-boxing features working together.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <input type="button" onclick="testFunction()" value="Test">
    `);
  await TestRunner.addScriptTag('../debugger/resources/framework.js');
  await TestRunner.evaluateInPagePromise(`
      window.callbackFromFramework = function(next)
      {
          return next();
      }

      function testFunction()
      {
          setTimeout(timeout1, 0);
      }

      function timeout1()
      {
          Framework.safeRun(Framework.empty, callback1);
      }

      function callback1()
      {
          Framework.doSomeAsyncChainCalls(callback2);
      }

      function callback2()
      {
          debugger;
      }
  `);

  var frameworkRegexString = '/framework\\.js$';
  var maxAsyncCallStackDepth = 8;

  Common.Settings.settingForTest('skip-stack-frames-pattern').set(frameworkRegexString);

  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    TestRunner.DebuggerAgent.setAsyncCallStackDepth(maxAsyncCallStackDepth).then(step2);
  }

  function step2() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(didPause);
  }

  async function didPause(callFrames, reason, breakpointIds, asyncStackTrace) {
    await SourcesTestRunner.captureStackTrace(callFrames, asyncStackTrace, {'dropFrameworkCallFrames': false});
    TestRunner.addResult('\nPrinting visible call stack:');
    await SourcesTestRunner.captureStackTrace(callFrames, asyncStackTrace, {'dropFrameworkCallFrames': true});
    SourcesTestRunner.completeDebuggerTest();
  }
})();
