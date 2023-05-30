// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that all inlined scripts from the same document are shown in the same source frame with html script tags.`);
  await TestRunner.loadLegacyModule('sources');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise('resources/debug-inline-scripts.html');

  var panel = UI.panels.sources;
  SourcesTestRunner.startDebuggerTest(step1, true);

  function callstackStatus() {
    var statusElement = panel.element.querySelector('.paused-message');
    return statusElement ? statusElement.deepTextContent() : 'not found';
  }

  function step1() {
    SourcesTestRunner.showScriptSource('debug-inline-scripts.html', step2);
  }

  async function step2(sourceFrame) {
    TestRunner.addResult('Script source was shown.');

    await SourcesTestRunner.setBreakpoint(sourceFrame, 2, '', true);
    await SourcesTestRunner.setBreakpoint(sourceFrame, 9, '', true);

    SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame).then(() => {
      TestRunner.reloadPage(() => SourcesTestRunner.completeDebuggerTest());
    });

    SourcesTestRunner.waitUntilPaused(step3);
  }

  async function step3(callFrames) {
    TestRunner.addResult('Script execution paused.');
    await SourcesTestRunner.captureStackTrace(callFrames);
    TestRunner.addResult('Call stack status: ' + callstackStatus());
    SourcesTestRunner.showScriptSource('debug-inline-scripts.html', step4);
  }

  function step4(sourceFrame) {
    SourcesTestRunner.dumpSourceFrameContents(sourceFrame);
    SourcesTestRunner.resumeExecution(
        SourcesTestRunner.waitUntilPaused.bind(null, step5));
  }

  async function step5(callFrames) {
    if (callFrames[0].location().lineNumber !== 9) {
      SourcesTestRunner.resumeExecution(
          SourcesTestRunner.waitUntilPaused.bind(null, step5));
      return;
    }

    TestRunner.addResult('Script execution paused.');
    await SourcesTestRunner.captureStackTrace(callFrames);
    SourcesTestRunner.showScriptSource('debug-inline-scripts.html', step6);
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
