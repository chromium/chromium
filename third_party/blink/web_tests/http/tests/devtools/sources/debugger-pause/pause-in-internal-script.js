// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that internal scripts unknown to front-end are processed correctly when appear in debugger call frames. Bug 64995\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <p>Tests that internal scripts unknown to front-end are processed correctly when appear in debugger call frames.
      <a href="https://bugs.webkit.org/show_bug.cgi?id=64995">Bug 64995</a>
      </p>
    `);
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          var array = [2, 5, 7];
          var sum = 0;
          array.forEach(function(key)
          {
              sum += array[key];
          });
          return sum;
      }
  `);

  SourcesTestRunner.runDebuggerTestSuite([function testSetBreakpoint(next) {
    SourcesTestRunner.showScriptSource(
        'pause-in-internal-script.js', didShowScriptSource);

    var breakpointFunctionFrame = null;

    async function didShowScriptSource(sourceFrame) {
      breakpointFunctionFrame = sourceFrame;
      TestRunner.addResult('Script source was shown.');
      await SourcesTestRunner.setBreakpoint(sourceFrame, 24, '', true);
      SourcesTestRunner.runTestFunctionAndWaitUntilPaused(didPause);
    }

    async function didPause(callFrames) {
      await SourcesTestRunner.captureStackTrace(callFrames);
      SourcesTestRunner.removeBreakpoint(breakpointFunctionFrame, 24);
      next();
    }
  }]);
})();
