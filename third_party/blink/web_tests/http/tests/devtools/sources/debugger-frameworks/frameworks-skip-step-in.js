// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests the skip stack frames feature when stepping.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <input type="button" onclick="testFunction()" value="Test">
    `);
  await TestRunner.addScriptTag('../debugger/resources/framework.js');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          for (var i = 1, func; func = eval("typeof test" + i + " === 'function' && test" + i); ++i)
              func();
      }

      function callback()
      {
          return 0;
      }

      function test1()
      {
          debugger;
          Framework.empty();
      }

      function test2()
      {
          debugger;
          Framework.doSomeWork();
      }

      function test3()
      {
          debugger;
          Framework.safeRun(Framework.empty, callback); // Should step into callback
      }

      function test4()
      {
          debugger;
          Framework.safeRun(Framework.doSomeWork, callback); // Should step into callback
      }

      function test5()
      {
          debugger;
          Framework.safeRun(Framework.empty, Framework.throwFrameworkException, callback); // Should be enough to step into callback
      }

      function test6()
      {
          debugger;
          Framework.safeRun(Framework.doSomeWorkDoNotChangeTopCallFrame, callback);
      }
  `);

  var frameworkRegexString = '/framework\\.js$';
  var totalDebuggerStatements = 6;

  Common.Settings.settingForTest('skip-stack-frames-pattern').set(frameworkRegexString);

  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(didPause);
  }

  var step = 0;
  var stepInCount = 0;
  async function didPause(callFrames, reason, breakpointIds, asyncStackTrace) {
    if (stepInCount < 2) {
      ++stepInCount;
      SourcesTestRunner.stepInto();
      SourcesTestRunner.waitUntilResumed(SourcesTestRunner.waitUntilPaused.bind(SourcesTestRunner, didPause));
      return;
    }

    stepInCount = 0;
    await SourcesTestRunner.captureStackTrace(callFrames);
    TestRunner.addResult('');
    if (++step < totalDebuggerStatements)
      SourcesTestRunner.resumeExecution(SourcesTestRunner.waitUntilPaused.bind(SourcesTestRunner, didPause));
    else
      SourcesTestRunner.completeDebuggerTest();
  }
})();
