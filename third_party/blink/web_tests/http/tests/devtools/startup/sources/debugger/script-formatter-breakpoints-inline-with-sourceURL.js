// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.setupStartupTest('resources/script-formatter-breakpoints-inline-with-sourceURL.html');
  TestRunner.addResult(`Tests the script formatting is working with breakpoints for inline scripts with #sourceURL=.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  Bindings.breakpointManager._storage._breakpoints = new Map();
  let panel = UI.panels.sources;
  let scriptFormatter;
  let sourceFrame;
  let formattedSourceFrame;

  SourcesTestRunner.runDebuggerTestSuite([
    function testSetup(next) {
      SourcesTestRunner.scriptFormatter().then(function(sf) {
        scriptFormatter = sf;
        next();
      });
    },

    async function testBreakpointsInOriginalAndFormattedSource(next) {
      SourcesTestRunner.showScriptSource('named-inline-script.js', didShowScriptSource);

      function didShowScriptSource(frame) {
        sourceFrame = frame;
        SourcesTestRunner.setBreakpoint(sourceFrame, 4, '', true);  // Lines here are zero based.
        Promise.all([SourcesTestRunner.waitBreakpointSidebarPane(true), SourcesTestRunner.waitUntilPausedPromise()])
              .then(pausedInFunctionInInlineScriptWithSourceURL);
        TestRunner.evaluateInPageWithTimeout('functionInInlineScriptWithSourceURL()');
      }

      function pausedInFunctionInInlineScriptWithSourceURL(callFrames) {
        SourcesTestRunner.dumpBreakpointSidebarPane('while paused in raw');
        SourcesTestRunner.resumeExecution(resumed);
      }

      function resumed() {
        TestRunner.addSniffer(
            Sources.ScriptFormatterEditorAction.prototype, '_updateButton', uiSourceCodeScriptFormatted);
        scriptFormatter._toggleFormatScriptSource();
      }

      function uiSourceCodeScriptFormatted() {
        // There should be a breakpoint in functionInInlineScriptWithSourceURL although script is pretty-printed.
        Promise.all([SourcesTestRunner.waitBreakpointSidebarPane(true), SourcesTestRunner.waitUntilPausedPromise()])
            .then(pausedInFunctionInInlineScriptWithSourceURLAgain);
        TestRunner.evaluateInPageWithTimeout('functionInInlineScriptWithSourceURL()');
      }

      function pausedInFunctionInInlineScriptWithSourceURLAgain(callFrames) {
        SourcesTestRunner.dumpBreakpointSidebarPane('while paused in pretty printed');
        SourcesTestRunner.showScriptSource('named-inline-script.js:formatted', didShowFormattedScriptSource);
      }

      function didShowFormattedScriptSource(frame) {
        formattedSourceFrame = frame;
        SourcesTestRunner.removeBreakpoint(formattedSourceFrame, 2);  // Lines here are zero based.
        Sources.sourceFormatter.discardFormattedUISourceCode(formattedSourceFrame.uiSourceCode());
        SourcesTestRunner.waitBreakpointSidebarPane().then(onBreakpointsUpdated);
      }

      function onBreakpointsUpdated() {
        SourcesTestRunner.dumpBreakpointSidebarPane('while paused after removing breakpoint in pretty printed and closing pretty printed');
        SourcesTestRunner.setBreakpoint(sourceFrame, 4, '', true);  // Lines here are zero based.
        Promise.all([SourcesTestRunner.waitBreakpointSidebarPane(true), SourcesTestRunner.waitUntilPausedPromise()])
          .then(pausedInFunctionInInlineScriptWithSourceURLThirdTime);
        TestRunner.evaluateInPageWithTimeout('functionInInlineScriptWithSourceURL()');
      }

      function pausedInFunctionInInlineScriptWithSourceURLThirdTime() {
        SourcesTestRunner.dumpBreakpointSidebarPane('while paused in original script again');
        SourcesTestRunner.removeBreakpoint(sourceFrame, 4);  // Lines here are zero based.
        SourcesTestRunner.resumeExecution(next);
      }
    }
  ]);
})();
