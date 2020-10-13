// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the script formatting is working fine with breakpoints.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('../debugger/resources/unformatted2.js');

  Bindings.breakpointManager._storage._breakpoints = new Map();
  var panel = UI.panels.sources;
  var scriptFormatter;
  var formattedSourceFrame;

  SourcesTestRunner.runDebuggerTestSuite([
    function testSetup(next) {
      SourcesTestRunner.scriptFormatter().then(function(sf) {
        scriptFormatter = sf;
        next();
      });
    },

    function testBreakpointsSetAndRemoveInFormattedSource(next) {
      SourcesTestRunner.showScriptSource('unformatted2.js', didShowScriptSource);

      function didShowScriptSource(frame) {
        TestRunner.addSniffer(
            Sources.ScriptFormatterEditorAction.prototype, '_updateButton', uiSourceCodeScriptFormatted);
        scriptFormatter.toggleFormatScriptSource();
      }

      async function uiSourceCodeScriptFormatted() {
        formattedSourceFrame = panel.visibleView;
        await SourcesTestRunner.waitUntilDebuggerPluginLoaded(
            formattedSourceFrame);
        await SourcesTestRunner.setBreakpoint(formattedSourceFrame, 3, '', true);
        SourcesTestRunner.waitBreakpointSidebarPane().then(evaluateF2);
      }

      function evaluateF2() {
        SourcesTestRunner.waitUntilPaused(pausedInF2);
        TestRunner.evaluateInPageWithTimeout('f2()');
      }

      async function pausedInF2(callFrames) {
        SourcesTestRunner.dumpBreakpointSidebarPane('while paused in pretty printed');
        SourcesTestRunner.waitBreakpointSidebarPane()
            .then(() => SourcesTestRunner.dumpBreakpointSidebarPane('while paused in raw'))
            .then(() => SourcesTestRunner.resumeExecution(next));
        SourcesTestRunner.removeBreakpoint(formattedSourceFrame, 3);
        await Formatter.SourceFormatter.instance().discardFormattedUISourceCode(panel.visibleView.uiSourceCode());
      }
    }
  ]);
})();
