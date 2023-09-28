// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that all inlined modules from the same document are shown in the same source frame with html script tags. Bug 1338257.\n`);
  await TestRunner.showPanel('sources');

  await TestRunner.navigatePromise('resources/inline-modules.html');

  SourcesTestRunner.startDebuggerTest(step0, true);

  function step0() {
    TestRunner.evaluateInPage('window.location="#hash"', step1);
  }

  function step1(loc) {
    TestRunner.addResult('window.location: ' + loc);
    SourcesTestRunner.showScriptSource('inline-modules.html', step2);
  }

  async function step2(sourceFrame) {
    TestRunner.addResult('Script source was shown.');
    await SourcesTestRunner.setBreakpoint(sourceFrame, 6, '', true);
    SourcesTestRunner.waitUntilPaused(step3);
    TestRunner.reloadPage(
        SourcesTestRunner.completeDebuggerTest.bind(SourcesTestRunner));
  }

  async function step3(callFrames) {
    TestRunner.addResult('Script execution paused.');
    await SourcesTestRunner.captureStackTrace(callFrames);
    SourcesTestRunner.showScriptSource('inline-modules.html', step4);
  }

  function step4(sourceFrame) {
    SourcesTestRunner.dumpSourceFrameContents(sourceFrame);
    SourcesTestRunner.resumeExecution(
        SourcesTestRunner.waitUntilPaused.bind(null, step5));
  }

  async function step5(callFrames) {
    if (callFrames[0].location.lineNumber !== 10) {
      SourcesTestRunner.resumeExecution(
          SourcesTestRunner.waitUntilPaused.bind(null, step5));
      return;
    }
    TestRunner.addResult('Script execution paused.');
    await SourcesTestRunner.captureStackTrace(callFrames);
    SourcesTestRunner.showScriptSource('inline-modules.html', step6);
  }

  function step6(sourceFrame) {
    SourcesTestRunner.dumpSourceFrameContents(sourceFrame);
    SourcesTestRunner.resumeExecution(
        SourcesTestRunner.waitUntilPaused.bind(null, step7));
  }

  function step7() {
    SourcesTestRunner.resumeExecution(
        SourcesTestRunner.waitUntilPaused.bind(null, step5));
  }
})();
