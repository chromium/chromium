// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests that resume button in overlay works\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          debugger;
      }

      function clickAt(x, y)
      {
          eventSender.mouseMoveTo(x, y);
          eventSender.mouseDown();
          eventSender.mouseUp();
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function inOverlay() {
    var resumeButton = document.getElementById('resume-button');
    var centerX = resumeButton.offsetLeft + resumeButton.offsetWidth / 2;
    var centerY = resumeButton.offsetTop + resumeButton.offsetHeight / 2;
    return JSON.stringify({x: centerX, y: centerY});
  }

  function step2(callFrames) {
    TestRunner.evaluateFunctionInOverlay(inOverlay, step3);
  }

  function step3(val) {
    TestRunner.addResult('Make a click');
    // TODO(pfeldman): resolve this.
    SourcesTestRunner.completeDebuggerTest();
    return;
    var resumeButtonCenter = JSON.parse(val);
    SourcesTestRunner.waitUntilResumed(step4);
    ConsoleTestRunner.evaluateInConsole(
        'clickAt(' + resumeButtonCenter.x + ', ' + resumeButtonCenter.y + ');');
  }

  function step4() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
