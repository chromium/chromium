// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that breakpoints are correctly handled while debugger is turned off\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise(
      'resources/debugger-disable-add-breakpoint.html');

  SourcesTestRunner.startDebuggerTest(step1);
  var testSourceFrame;
  function step1() {
    SourcesTestRunner.showScriptSource(
        'debugger-disable-add-breakpoint.html', step2);
  }

  function step2(sourceFrame) {
    testSourceFrame = sourceFrame;
    TestRunner.addResult('Main resource was shown.');
    TestRunner.debuggerModel.addEventListener(
        SDK.DebuggerModel.Events.DebuggerWasDisabled, step3, this);
    TestRunner.debuggerModel.disableDebugger();
  }

  async function step3() {
    TestRunner.debuggerModel.removeEventListener(
        SDK.DebuggerModel.Events.DebuggerWasDisabled, step3, this);
    TestRunner.addResult('Debugger disabled.');
    await SourcesTestRunner.setBreakpoint(testSourceFrame, 3, '', true);
    TestRunner.addResult('Breakpoint added');
    await TestRunner.debuggerModel.enableDebugger();
    step4();
  }

  function step4() {
    TestRunner.addResult('Debugger was enabled');
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step5);
  }

  function step5() {
    SourcesTestRunner.resumeExecution(step6);
  }

  function step6() {
    TestRunner.addResult('Disable debugger again');
    TestRunner.debuggerModel.addEventListener(
        SDK.DebuggerModel.Events.DebuggerWasDisabled, step7, this);
    TestRunner.debuggerModel.disableDebugger();
  }

  function step7() {
    TestRunner.addResult('Debugger disabled');
    TestRunner.debuggerModel.removeEventListener(
      SDK.DebuggerModel.Events.DebuggerWasDisabled, step7, this);
    SourcesTestRunner.removeBreakpoint(testSourceFrame, 3);
    TestRunner.addResult('Breakpoint removed');
    TestRunner.debuggerModel.addEventListener(
        SDK.DebuggerModel.Events.DebuggerWasEnabled, step8, this);
    TestRunner.debuggerModel.enableDebugger();
  }

  function step8() {
    TestRunner.debuggerModel.removeEventListener(
      SDK.DebuggerModel.Events.DebuggerWasEnabled, step8, this);
    TestRunner.addResult('Debugger enabled');
    TestRunner.addResult('Evaluating test function.');
    TestRunner.evaluateInPage('testFunction()', step9);
  }

  function step9() {
    TestRunner.addResult(
        'function evaluated without a pause on the breakpoint.');
    SourcesTestRunner.completeDebuggerTest();
  }
})();
