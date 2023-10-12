// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that debugger will stop on "debugger" statement in a function that was added to the inspected page via evaluation in Web Inspector console.`);
  await TestRunner.showPanel('sources');
  var scriptToEvaluate = 'function testFunction() {\n' +
      '    debugger;\n' +
      '}\n' +
      'setTimeout(testFunction, 0);\n';

  SourcesTestRunner.startDebuggerTest(step1);

  async function step1() {
    await ConsoleTestRunner.evaluateInConsolePromise(scriptToEvaluate);
    SourcesTestRunner.waitUntilPaused(step2);
  }

  async function step2(callFrames) {
    await SourcesTestRunner.captureStackTrace(callFrames);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
