// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests event listener breakpoint to break on the first statement of new scripts.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <input type="button" onclick="testFunction()" value="Test">
    `);
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          setTimeout(callback1, 0);
      }

      function callback1()
      {
          var code =
              "(function() {\\n" +
                  "setTimeout(callback2, 0);\\n" +
              "})();\\n" +
              "//# sourceURL=inline_callback1.js";
          setTimeout(code, 0);
      }

      function callback2()
      {
          var script = document.createElement("script");
          script.src = "../debugger/resources/script1.js";
          document.body.appendChild(script);
          script.remove();
      }
  `);

  var numberOfPauses = 2;

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    SourcesTestRunner.setEventListenerBreakpoint('instrumentation:scriptFirstStatement', true);
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(didPause);
  }

  async function didPause(callFrames, reason, breakpointIds, asyncStackTrace, auxData) {
    var eventName = (auxData && auxData.eventName || '').replace(/^instrumentation:/, '');
    TestRunner.addResult('\nPaused on ' + eventName);
    await SourcesTestRunner.captureStackTrace(callFrames);

    if (--numberOfPauses)
      SourcesTestRunner.resumeExecution(SourcesTestRunner.waitUntilPaused.bind(SourcesTestRunner, didPause));
    else
      SourcesTestRunner.resumeExecution(step2);
  }

  function step2() {
    SourcesTestRunner.setEventListenerBreakpoint('instrumentation:scriptFirstStatement', false);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
