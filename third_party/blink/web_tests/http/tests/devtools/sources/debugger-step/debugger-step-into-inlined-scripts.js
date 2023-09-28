// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that debugger StepInto will step through inlined scripts.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise(
      'resources/debugger-step-into-inlined-scripts.html');

  var numberOfStepInto = 6;

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    SourcesTestRunner.showScriptSource(
        'debugger-step-into-inlined-scripts.html', step2);
  }

  async function step2(sourceFrame) {
    TestRunner.addResult('Script source was shown.');
    await SourcesTestRunner.setBreakpoint(sourceFrame, 6, '', true);
    SourcesTestRunner.waitUntilPaused(step3);
    TestRunner.reloadPage(completeTest);
  }

  function step3() {
    var actions = ['Print'];  // First pause on breakpoint.
    for (var i = 0; i < numberOfStepInto; ++i)
      actions.push('StepInto', 'Print');
    actions.push('Resume');
    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(actions);
  }

  function completeTest() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
