// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that 'skip all pauses' mode blocks breakpoint and gets cancelled right at page reload.`);
  await TestRunner.showPanel('sources');

  await TestRunner.navigatePromise('resources/skip-pauses-until-reload.html')

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.showScriptSource(
        'skip-pauses-until-reload.html', didShowScriptSource);
  }

  async function didShowScriptSource(sourceFrame) {
    TestRunner.addResult('Script source was shown.');
    TestRunner.addResult('Set up breakpoints.');
    await SourcesTestRunner.setBreakpoint(sourceFrame, 8, '', true);
    await SourcesTestRunner.setBreakpoint(sourceFrame, 9, '', true);
    TestRunner.addResult('Set up to pause on all exceptions.');
    // FIXME: Test is flaky with PauseOnAllExceptions due to races in debugger.
    TestRunner.DebuggerAgent.setPauseOnExceptions(
        SDK.DebuggerModel.PauseOnExceptionsState.DontPauseOnExceptions);
    ElementsTestRunner.nodeWithId('element', didResolveNode);
    testRunner.logToStderr('didShowScriptSource');
  }

  function didResolveNode(node) {
    testRunner.logToStderr('didResolveNode');
    TestRunner.addResult('Set up DOM breakpoints.');
    TestRunner.domDebuggerModel.setDOMBreakpoint(
        node, Protocol.DOMDebugger.DOMBreakpointType.SubtreeModified);
    TestRunner.domDebuggerModel.setDOMBreakpoint(
        node, Protocol.DOMDebugger.DOMBreakpointType.AttributeModified);
    TestRunner.domDebuggerModel.setDOMBreakpoint(
        node, Protocol.DOMDebugger.DOMBreakpointType.NodeRemoved);
    setUpEventBreakpoints();
  }

  function setUpEventBreakpoints() {
    testRunner.logToStderr('setUpEventBreakpoints');
    TestRunner.addResult('Set up Event breakpoints.');
    SourcesTestRunner.setEventListenerBreakpoint('listener:click', true);
    TestRunner.deprecatedRunAfterPendingDispatches(didSetUp);
  }

  function didSetUp() {
    testRunner.logToStderr('didSetUp');
    TestRunner.addResult('Did set up.');
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(didPause);
  }

  async function didPause(callFrames) {
    testRunner.logToStderr('didPause');
    await SourcesTestRunner.captureStackTrace(callFrames);
    TestRunner.DebuggerAgent.setSkipAllPauses(true).then(didSetSkipAllPauses);
  }

  function didSetSkipAllPauses() {
    testRunner.logToStderr('didSetSkipAllPauses');
    TestRunner.addResult('Set up to skip all pauses.');
    doReloadPage();
  }

  function doReloadPage() {
    testRunner.logToStderr('doReloadPage');
    TestRunner.addResult('Reloading the page...');
    SourcesTestRunner.waitUntilPausedNextTime(didPauseAfterReload);
    TestRunner.reloadPage(didReloadPage);
  }

  function didReloadPage() {
    testRunner.logToStderr('didReloadPage');
    TestRunner.addResult('PASS: Reloaded without hitting breakpoints.');
    completeTest();
  }

  async function didPauseAfterReload(callFrames) {
    testRunner.logToStderr('didPauseAfterReload');
    TestRunner.addResult('FAIL: Should not pause while reloading the page!');
    await SourcesTestRunner.captureStackTrace(callFrames);
    SourcesTestRunner.waitUntilPausedNextTime(didPauseAfterReload);
    SourcesTestRunner.resumeExecution();
  }

  function completeTest() {
    testRunner.logToStderr('completeTest');
    SourcesTestRunner.setEventListenerBreakpoint('listener:click', false);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
