// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that browser won't crash if user evaluates something in the console that would suspend active dom objects (e.g. if user attempts to show an alert) when script execution is paused on a breakpoint and all active dom objects are already suspended.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction() {
          setTimeout("void 0", 0); // Create a timer that will be suspended on a breakpoint.
          debugger;
      }
  `);

  SourcesTestRunner.startDebuggerTest(startDebuggerTestCallback);

  function startDebuggerTestCallback() {
    TestRunner.evaluateInPage('setTimeout(testFunction, 0)', function(result) {
      TestRunner.addResult('Set timer for test function.');
    });

    SourcesTestRunner.waitUntilPaused(function(callFrames) {
      TestRunner.evaluateInPage('alert(1)', function(result) {
        TestRunner.addResult('Shown alert while staying on a breakpoint.');
        SourcesTestRunner.completeDebuggerTest();
      });
    });
  }
})();
