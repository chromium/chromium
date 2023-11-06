// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests event listener breakpoint to break on the first statement of new modules.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          var module = document.createElement("script");
          module.type = "module";
          module.src = "./resources/empty-module.js";
          document.body.appendChild(module);
          module.remove();
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    SourcesTestRunner.setEventListenerBreakpoint('instrumentation:scriptFirstStatement', true);
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(didPause);
  }

  async function didPause(callFrames, reason, breakpointIds, asyncStackTrace, auxData) {
    var eventName = (auxData && auxData.eventName || '').replace(/^instrumentation:/, '');
    TestRunner.addResult('\nPaused on ' + eventName);
    await SourcesTestRunner.captureStackTrace(callFrames);
    SourcesTestRunner.resumeExecution(step2);
  }

  function step2() {
    SourcesTestRunner.setEventListenerBreakpoint('instrumentation:scriptFirstStatement', false);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
