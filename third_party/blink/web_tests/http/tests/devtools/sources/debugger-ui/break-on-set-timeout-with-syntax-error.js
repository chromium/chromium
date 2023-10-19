// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that scheduled pause is cleared after processing event with empty handler.\n`);
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
