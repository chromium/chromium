// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests setting logpoints.\n`);
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise('resources/set-breakpoint.html');

  var currentSourceFrame;
  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.runDebuggerTestSuite([
    function testSetLogpoint(next) {
      SourcesTestRunner.showScriptSource(
          'set-breakpoint.html', didShowScriptSource);

      function didShowScriptSource(sourceFrame) {
        currentSourceFrame = sourceFrame;
        TestRunner.addResult('Script source was shown.');
        const condition = Sources.BreakpointEditDialog._conditionForLogpoint(`"x is", x`);
        SourcesTestRunner
            .createNewBreakpoint(currentSourceFrame, 14, condition, true)
            .then(() => SourcesTestRunner.waitBreakpointSidebarPane())
            .then(() => SourcesTestRunner.runTestFunction())
            .then(testFunctionFinished);
      }

      async function testFunctionFinished(callFrames) {
        TestRunner.addResult('Test function finished.');
        SourcesTestRunner.dumpBreakpointSidebarPane();

        await ConsoleTestRunner.waitForConsoleMessagesPromise(1);
        await ConsoleTestRunner.dumpConsoleMessages();

        SourcesTestRunner.waitBreakpointSidebarPane().then(breakpointRemoved);
        SourcesTestRunner.removeBreakpoint(currentSourceFrame, 14);
      }

      function breakpointRemoved() {
        TestRunner.addResult('Breakpoints removed.');
        SourcesTestRunner.dumpBreakpointSidebarPane();
        next();
      }
    },
  ]);
})();
