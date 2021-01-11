// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests setting breakpoints.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise('resources/set-breakpoint.html');

  var currentSourceFrame;
  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.runDebuggerTestSuite([
    function testSetConditionalBreakpointThatBreaks(next) {
      SourcesTestRunner.showScriptSource(
          'set-breakpoint.html', didShowScriptSource);

      function didShowScriptSource(sourceFrame) {
        currentSourceFrame = sourceFrame;
        TestRunner.addResult('Script source was shown.');
        SourcesTestRunner.waitUntilPaused(didPause);
        SourcesTestRunner
            .createNewBreakpoint(currentSourceFrame, 13, 'true', true)
            .then(() => SourcesTestRunner.waitBreakpointSidebarPane())
            .then(() => setTimeout(() =>
                SourcesTestRunner.runTestFunction(), 1));
      }

      async function didPause(callFrames) {
        TestRunner.addResult('Script execution paused.');
        await SourcesTestRunner.captureStackTrace(callFrames);
        SourcesTestRunner.dumpBreakpointSidebarPane();
        SourcesTestRunner.waitBreakpointSidebarPane().then(breakpointRemoved);
        SourcesTestRunner.removeBreakpoint(currentSourceFrame, 13);
      }

      function breakpointRemoved() {
        SourcesTestRunner.resumeExecution(didResume);
      }

      function didResume() {
        SourcesTestRunner.dumpBreakpointSidebarPane();
        TestRunner.addResult('Script execution resumed.');
        next();
      }
    },

    function testSetConditionalBreakpointThatDoesNotBreak(next) {
      SourcesTestRunner.showScriptSource(
          'set-breakpoint.html', didShowScriptSource);

      function didShowScriptSource(sourceFrame) {
        currentSourceFrame = sourceFrame;
        TestRunner.addResult('Script source was shown.');
        SourcesTestRunner
            .createNewBreakpoint(currentSourceFrame, 13, 'false', true)
            .then(() => SourcesTestRunner.waitBreakpointSidebarPane())
            .then(() => SourcesTestRunner.runTestFunction())
            .then(testFunctionFinished);
      }

      function testFunctionFinished(callFrames) {
        TestRunner.addResult('Test function finished.');
        SourcesTestRunner.dumpBreakpointSidebarPane();
        SourcesTestRunner.waitBreakpointSidebarPane().then(breakpointRemoved);
        SourcesTestRunner.removeBreakpoint(currentSourceFrame, 13);
      }

      function breakpointRemoved() {
        TestRunner.addResult('Breakpoints removed.');
        SourcesTestRunner.dumpBreakpointSidebarPane();
        next();
      }
    },
  ]);
})();
