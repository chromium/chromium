// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that debugger StepOut will skip inlined scripts created by document.write().\n`);
  await TestRunner.showPanel('sources');

  var numberOfStepOut = 5;
  await SourcesTestRunner.startDebuggerTestPromise(true);
  await TestRunner.navigatePromise(
      'resources/debugger-step-out-document-write.html');
  SourcesTestRunner.showScriptSource(
      'debugger-step-out-document-write.html', step2);

  async function step2(sourceFrame) {
    TestRunner.addResult('Script source was shown.');
    await SourcesTestRunner.setBreakpoint(sourceFrame, 3, '', true);
    await SourcesTestRunner.setBreakpoint(sourceFrame, 11, '', true);
    SourcesTestRunner.waitUntilPaused(step3);
    TestRunner.reloadPage(completeTest);
  }

  function step3() {
    var actions = ['Print'];  // First pause on breakpoint.
    for (var i = 0; i < numberOfStepOut; ++i)
      actions.push('StepOut', 'Print');
    actions.push('Resume');
    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(actions);
  }

  function completeTest() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
