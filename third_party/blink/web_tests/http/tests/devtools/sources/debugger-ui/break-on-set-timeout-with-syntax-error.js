// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that scheduled pause is cleared after processing event with empty handler.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadLegacyModule('panels/browser_debugger'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function runSetTimeoutWithSyntaxError()
      {
          setTimeout({}, 0);
      }

      function executeSomeCode()
      {
          debugger; // should stop here not earlier
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    SourcesTestRunner.setEventListenerBreakpoint('instrumentation:setTimeout.callback', true);
    TestRunner.evaluateInPage(
        'runSetTimeoutWithSyntaxError()', ConsoleTestRunner.waitUntilMessageReceived.bind(this, step2));
  }

  function step2() {
    var actions = ['Print'];
    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(actions, step3);
    TestRunner.evaluateInPage('executeSomeCode()');
  }

  function step3() {
    SourcesTestRunner.setEventListenerBreakpoint('instrumentation:setTimeout.callback', false);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
