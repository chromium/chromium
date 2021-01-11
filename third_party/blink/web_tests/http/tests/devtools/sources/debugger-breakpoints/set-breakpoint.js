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
    function testSetBreakpoint(next) {
      SourcesTestRunner.showScriptSource(
          'set-breakpoint.html', didShowScriptSource);

      function didShowScriptSource(sourceFrame) {
        currentSourceFrame = sourceFrame;
        TestRunner.addResult('Script source was shown.');
        SourcesTestRunner.waitUntilPaused(didPause);
        SourcesTestRunner.createNewBreakpoint(currentSourceFrame, 13, '', true)
            .then(() => SourcesTestRunner.waitBreakpointSidebarPane())
            .then(() => setTimeout(() =>
                SourcesTestRunner.runTestFunction(), 1));
      }

      async function didPause(callFrames) {
        TestRunner.addResult('Script execution paused.');
        await SourcesTestRunner.captureStackTrace(callFrames);
        SourcesTestRunner.dumpBreakpointSidebarPane();
        SourcesTestRunner.removeBreakpoint(currentSourceFrame, 13);
        SourcesTestRunner.waitBreakpointSidebarPane().then(breakpointRemoved);
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

    function testSetBreakpointOnTheLastLine(next) {
      SourcesTestRunner.showScriptSource(
          'set-breakpoint.html', didShowScriptSource);

      function didShowScriptSource(sourceFrame) {
        currentSourceFrame = sourceFrame;
        SourcesTestRunner.waitUntilPaused(didPause);
        SourcesTestRunner.createNewBreakpoint(currentSourceFrame, 3, '', true)
            .then(() => SourcesTestRunner.waitBreakpointSidebarPane())
            .then(
                () => TestRunner.evaluateInPage(
                    'setTimeout(oneLineTestFunction, 0)'));
      }

      async function didPause(callFrames) {
        await SourcesTestRunner.captureStackTrace(callFrames);
        SourcesTestRunner.removeBreakpoint(currentSourceFrame, 3);
        SourcesTestRunner.resumeExecution(next);
      }
    },

    function testSetBreakpointOnTheLastLine2(next) {
      SourcesTestRunner.showScriptSource(
          'set-breakpoint.html', didShowScriptSource);

      async function didShowScriptSource(sourceFrame) {
        currentSourceFrame = sourceFrame;
        await SourcesTestRunner.setBreakpoint(currentSourceFrame, 7, '', true);
        SourcesTestRunner.waitUntilPaused(didPause);
        TestRunner.evaluateInPage('setTimeout(oneLineTestFunction2, 0)');
      }

      async function didPause(callFrames) {
        await SourcesTestRunner.captureStackTrace(callFrames);
        SourcesTestRunner.removeBreakpoint(currentSourceFrame, 7);
        SourcesTestRunner.resumeExecution(next);
      }
    },

    async function testSetBreakpointOnTheSameLine(next) {
      var breakpointId = await TestRunner.DebuggerAgent.setBreakpointByUrl(
          1, 'foo.js', undefined, undefined, 2, '');
      TestRunner.assertTrue(!!breakpointId);

      breakpointId = await TestRunner.DebuggerAgent.setBreakpointByUrl(
          1, 'foo.js', undefined, undefined, 2, '');
      TestRunner.assertTrue(!breakpointId);

      next();
    }
  ]);
})();
