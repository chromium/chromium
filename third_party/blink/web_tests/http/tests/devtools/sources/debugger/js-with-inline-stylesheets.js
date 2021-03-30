// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that JS sourcemapping for inline scripts followed by inline stylesheets does not break.\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          var x = Math.sqrt(10);
          return x;
      }
  `);

  var currentSourceFrame;
  SourcesTestRunner.setQuiet(true);
  var pageURL = 'js-with-inline-stylesheets.js';
  SourcesTestRunner.runDebuggerTestSuite([function testSetBreakpoint(next) {
    SourcesTestRunner.showScriptSource(pageURL, didShowScriptSource);

    function didShowScriptSource(sourceFrame) {
      currentSourceFrame = sourceFrame;
      TestRunner.addResult('Script source was shown.');
      SourcesTestRunner.waitUntilPaused(didPause);
      SourcesTestRunner.createNewBreakpoint(currentSourceFrame, 12, '', true)
          .then(() => SourcesTestRunner.waitBreakpointSidebarPane())
          .then(() => SourcesTestRunner.runTestFunction());
    }

    async function didPause(callFrames) {
      TestRunner.addResult('Script execution paused.');
      await SourcesTestRunner.captureStackTrace(callFrames);
      SourcesTestRunner.dumpBreakpointSidebarPane();
      SourcesTestRunner.waitBreakpointSidebarPane().then(breakpointRemoved);
      SourcesTestRunner.removeBreakpoint(currentSourceFrame, 12);
    }

    function breakpointRemoved() {
      SourcesTestRunner.resumeExecution(didResume);
    }

    function didResume() {
      SourcesTestRunner.dumpBreakpointSidebarPane();
      TestRunner.addResult('Script execution resumed.');
      next();
    }
  }]);
})();
