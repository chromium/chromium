// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that reloading while paused at a breakpoint doesn't execute code after the breakpoint.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await SourcesTestRunner.startDebuggerTestPromise(true);
  TestRunner.addResult('Setting break on all exceptions.');
  TestRunner.DebuggerAgent.setPauseOnExceptions(
      SDK.DebuggerModel.PauseOnExceptionsState.PauseOnAllExceptions);
  SourcesTestRunner.waitUntilPaused(onPaused);
  await TestRunner.navigatePromise(
      '../../sources/debugger-breakpoints/resources/diverge-without-breakpoint-throw-on-load.html');
  TestRunner.evaluateInPageWithTimeout(`divergingFunctionWithThrow()`);

  async function onPaused(callFrames) {
    await SourcesTestRunner.captureStackTrace(callFrames);
    TestRunner.addResult('Reloading page...');
    TestRunner.reloadPage(onPageReloaded);
  }

  function onPageReloaded() {
    TestRunner.completeTest();
  }
})();
