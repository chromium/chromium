// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  // This test is testing the old breakpoint sidebar pane. Make sure to
  // turn off the new breakpoint pane experiment.
  Root.Runtime.experiments.setEnabled('breakpointView', false);
  TestRunner.addResult(`Tests disabling breakpoints.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          var x = Math.sqrt(10);
          console.log("Done.");
          return x;
      }
  `);

  var currentSourceFrame;
  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.runDebuggerTestSuite([
    function testSetBreakpointPauseResumeThenDisable(next) {
      SourcesTestRunner.showScriptSource('disable-breakpoints.js', didShowScriptSource);

      function didShowScriptSource(sourceFrame) {
        TestRunner.addResult('Script source was shown.');
        SourcesTestRunner.waitUntilPaused(didPause);
        SourcesTestRunner.createNewBreakpoint(sourceFrame, 12, '', true)
            .then(() => SourcesTestRunner.waitBreakpointSidebarPane())
            .then(() => SourcesTestRunner.runTestFunction());
      }

      async function didPause(callFrames) {
        TestRunner.addResult('Script execution paused.');
        await SourcesTestRunner.captureStackTrace(callFrames);
        SourcesTestRunner.dumpBreakpointSidebarPane();
        ConsoleTestRunner.addConsoleSniffer(testFunctionFinishedForTheFirstTime);
        SourcesTestRunner.resumeExecution(didResume);
      }

      function didResume(callFrames) {
        TestRunner.addResult('Script execution resumed.');
      }

      function testFunctionFinishedForTheFirstTime() {
        TestRunner.addResult('Test function finished.');

        TestRunner.addResult('Disabling breakpoints...');
        Common.moduleSetting('breakpointsActive').set(false);

        TestRunner.addResult('Running test function again...');
        ConsoleTestRunner.addConsoleSniffer(testFunctionFinishedForTheSecondTime);
        SourcesTestRunner.runTestFunction();
      }

      function testFunctionFinishedForTheSecondTime(callFrames) {
        TestRunner.addResult('Test function finished.');
        next();
      }
    },

    function testEnableBreakpointsAgain(next) {
      SourcesTestRunner.showScriptSource('disable-breakpoints.js', didShowScriptSource);

      function didShowScriptSource(sourceFrame) {
        currentSourceFrame = sourceFrame;
        TestRunner.addResult('Enabling breakpoints...');
        Common.moduleSetting('breakpointsActive').set(true);

        TestRunner.addResult('Running test function...');
        SourcesTestRunner.runTestFunctionAndWaitUntilPaused(didPause);
      }

      async function didPause(callFrames) {
        TestRunner.addResult('Script execution paused.');
        await SourcesTestRunner.captureStackTrace(callFrames);
        SourcesTestRunner.dumpBreakpointSidebarPane();
        ConsoleTestRunner.addConsoleSniffer(testFunctionFinished);
        SourcesTestRunner.resumeExecution(didResume);
      }

      function didResume(callFrames) {
        TestRunner.addResult('Script execution resumed.');
      }

      function testFunctionFinished() {
        TestRunner.addResult('Test function finished.');
        SourcesTestRunner.dumpBreakpointSidebarPane();
        SourcesTestRunner.waitBreakpointSidebarPane().then(breakpointRemoved);
        SourcesTestRunner.removeBreakpoint(currentSourceFrame, 12);
      }

      function breakpointRemoved() {
        TestRunner.addResult('Breakpoints removed.');
        SourcesTestRunner.dumpBreakpointSidebarPane();
        next();
      }
    },
  ]);
})();
