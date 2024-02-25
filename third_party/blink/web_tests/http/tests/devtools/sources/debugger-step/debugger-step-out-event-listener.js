// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that stepping out of an event listener will lead to a pause in the next event listener.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <div id="myDiv"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          function inner()
          {
              var div = document.getElementById("myDiv");
              function fooEventHandler1()
              {
                  div.textContent += "Recieved foo event(1)!\\n";
              }

              function fooEventHandler2()
              {
                  div.textContent += "Recieved foo event(2)!\\n";
              }

              div.addEventListener("foo", fooEventHandler1);
              div.addEventListener("foo", fooEventHandler2);

              var e = new CustomEvent("foo");
              debugger;
              div.dispatchEvent(e);

              div.removeEventListener("foo", fooEventHandler1);
              div.removeEventListener("foo", fooEventHandler2);
          }
          inner();
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2() {
    var actions = [
      'Print',  // debugger;
      'StepInto',
      'StepInto',
      'StepInto',
      'Print',  // at fooEventHandler1
      'StepOut',
      'Print',  // should be at fooEventHandler2
      'StepOut',
      'Print',
    ];
    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(actions, step3);
  }

  function step3() {
    SourcesTestRunner.resumeExecution(step4);
  }

  function step4() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step5);
  }

  function step5() {
    var actions = [
      'Print',             // debugger;
      'StepOut', 'Print',  // should be at inner()
    ];
    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(actions, step6);
  }

  function step6() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
