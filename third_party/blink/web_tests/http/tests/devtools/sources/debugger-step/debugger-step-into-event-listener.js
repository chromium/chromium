// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that stepping into dispatchEvent() method will lead to a pause in the first event listener.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <div id="myDiv"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          var div = document.getElementById("myDiv");
          function fooEventHandler1()
          {
              div.textContent += "Recieved foo event(1)!\\n";
          }
          div.addEventListener("foo", fooEventHandler1);

          function fooEventHandler2()
          {
              div.textContent += "Recieved foo event(2)!\\n";
          }
          div.addEventListener("foo", fooEventHandler2);

          var e = new CustomEvent("foo");
          debugger;
          div.dispatchEvent(e);
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function checkTopFrameFunction(callFrames, expectedName, reason) {
    var topFunctionName = callFrames[0].functionName;
    if (expectedName === topFunctionName)
      TestRunner.addResult(
          'SUCCESS: Did step into event listener(' + expectedName + ').');
    else
      TestRunner.addResult(
          'FAIL: Unexpected top function: expected ' + expectedName +
          ', found ' + topFunctionName);
    TestRunner.assertEquals(
        Protocol.Debugger.PausedEventReason.Step, reason,
        'FAIL: wrong pause reason: ' + reason);
  }

  var stepCount = 0;
  function step2(callFrames, reason) {
    if (stepCount === 2)
      checkTopFrameFunction(callFrames, 'fooEventHandler1', reason);
    else if (stepCount === 5)
      checkTopFrameFunction(callFrames, 'fooEventHandler2', reason);

    if (stepCount < 5) {
      TestRunner.addResult('Stepping into...');
      SourcesTestRunner.stepInto();
      SourcesTestRunner.waitUntilResumed(
          SourcesTestRunner.waitUntilPaused.bind(SourcesTestRunner, step2));
    } else {
      SourcesTestRunner.completeDebuggerTest();
    }
    stepCount++;
  }
})();
