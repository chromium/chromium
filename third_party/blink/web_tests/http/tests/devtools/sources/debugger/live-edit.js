// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests live edit feature.\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/edit-me.js');
  await TestRunner.addScriptTag('resources/edit-me-2.js');
  await TestRunner.addScriptTag('resources/edit-me-when-paused.js');

  var panel = UI.panels.sources;

  SourcesTestRunner.runDebuggerTestSuite([
    function testLiveEdit(next) {
      SourcesTestRunner.showScriptSource('edit-me.js', didShowScriptSource);

      function didShowScriptSource(sourceFrame) {
        replaceInSource(
            sourceFrame, 'return 0;', 'return "live-edited string";',
            didEditScriptSource);
      }

      async function didEditScriptSource() {
        var result = await TestRunner.evaluateInPageRemoteObject('f()');
        TestRunner.assertEquals(
            'live-edited string', result.description,
            'edited function returns wrong result');
        SourcesTestRunner.dumpSourceFrameContents(panel.visibleView);
        next();
      }
    },

    async function testLiveEditSyntaxError(next) {
      await TestRunner.addScriptTag('resources/edit-me-syntax-error.js');
      SourcesTestRunner.showScriptSource(
          'edit-me-syntax-error.js', didShowScriptSource);

      function didShowScriptSource(sourceFrame) {
        SourcesTestRunner.replaceInSource(
            sourceFrame, ',"I\'m good"', '"I\'m good"');
        SourcesTestRunner.dumpSourceFrameContents(panel.visibleView);
        next();
      }
    },

    function testBreakpointsUpdated(next) {
      var testSourceFrame;
      SourcesTestRunner.showScriptSource('edit-me.js', didShowScriptSource);

      async function didShowScriptSource(sourceFrame) {
        testSourceFrame = sourceFrame;
        await SourcesTestRunner.waitUntilDebuggerPluginLoaded(sourceFrame);
        SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame)
            .then(breakpointAdded);
        await SourcesTestRunner.setBreakpoint(sourceFrame, 2, '', true);
      }

      function breakpointAdded() {
        replaceInSource(
            panel.visibleView, 'function f()', 'var a = 1;\nfunction f()',
            didEditScriptSource);
      }

      function didEditScriptSource() {
        SourcesTestRunner.waitDebuggerPluginBreakpoints(testSourceFrame)
            .then(
                () => SourcesTestRunner.dumpDebuggerPluginBreakpoints(
                    testSourceFrame))
            .then(
                () => Bindings.breakpointManager.allBreakpointLocations().map(
                    breakpointLocation => breakpointLocation.breakpoint.remove()))
            .then(next);
      }
    },

    function testNoCrashWhenLiveEditOnBreakpoint(next) {
      SourcesTestRunner.showScriptSource('edit-me-2.js', didShowScriptSource);

      var testSourceFrame;

      async function didShowScriptSource(sourceFrame) {
        testSourceFrame = sourceFrame;
        await SourcesTestRunner.waitUntilDebuggerPluginLoaded(sourceFrame);
        SourcesTestRunner.waitDebuggerPluginBreakpoints(testSourceFrame)
            .then(breakpointAdded);
        await SourcesTestRunner.setBreakpoint(sourceFrame, 2, '', true);
      }

      function breakpointAdded() {
        SourcesTestRunner.waitUntilPaused(pausedInF);
        TestRunner.evaluateInPage('setTimeout(editMe2F, 0)');
      }

      function pausedInF(callFrames) {
        replaceInSource(
            panel.visibleView, 'function editMe2F()', 'function editMe2F()\n',
            didEditScriptSource);
      }

      function didEditScriptSource() {
        SourcesTestRunner.resumeExecution(resumed);
      }

      function resumed() {
        next();
      }
    }
  ]);

  function replaceInSource(sourceFrame, string, replacement, callback) {
    TestRunner.addSniffer(
        TestRunner.debuggerModel, '_didEditScriptSource', callback);
    SourcesTestRunner.replaceInSource(sourceFrame, string, replacement);
    SourcesTestRunner.commitSource(sourceFrame);
  }
})();
