// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that breakpoints are successfully restored after debugger disabling.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          return 0;
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.showScriptSource('debugger-disable-enable.js', step2);
  }

  async function step2(sourceFrame) {
    TestRunner.addResult('Main resource was shown.');
    await SourcesTestRunner.setBreakpoint(sourceFrame, 11, '', true);
    TestRunner.debuggerModel.addEventListener(SDK.DebuggerModel.Events.DebuggerWasDisabled, step3, this);
    TestRunner.debuggerModel.disableDebugger();
  }

  function step3() {
    TestRunner.debuggerModel.removeEventListener(SDK.DebuggerModel.Events.DebuggerWasDisabled, step3, this);
    TestRunner.addResult('Debugger disabled.');
    TestRunner.addResult('Evaluating test function.');
    TestRunner.evaluateInPage('testFunction()', step4);
  }

  function step4() {
    TestRunner.addResult('Function evaluated without a pause on the breakpoint.');
    TestRunner.addSniffer(TestRunner.debuggerModel, "setBreakpointByURL", step5);
    TestRunner.addResult('Enabling debugger.');
    TestRunner.debuggerModel.enableDebugger();
  }

  function step5() {
    TestRunner.addResult('Breakpoint was set after re-enabling.');
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step6);
  }

  function step6() {
    TestRunner.addResult('Function evaluated and paused on breakpoint.');
    SourcesTestRunner.completeDebuggerTest();
  }
})();
