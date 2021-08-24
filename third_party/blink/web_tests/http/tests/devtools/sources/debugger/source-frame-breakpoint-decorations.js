// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Checks that JavaScriptSourceFrame show breakpoints correctly\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/edit-me-breakpoints.js');

  Bindings.breakpointManager.storage.breakpoints = new Map();
  SourcesTestRunner.runDebuggerTestSuite([
    function testAddRemoveBreakpoint(next) {
      var javaScriptSourceFrame;
      SourcesTestRunner.showScriptSource('edit-me-breakpoints.js', addBreakpoint);

      function addBreakpoint(sourceFrame) {
        javaScriptSourceFrame = sourceFrame;
        TestRunner.addResult('Setting breakpoint');
        // Breakpoint decoration expectations are pairs of line number plus
        // breakpoint decoration counts. We expect line 2 to have 2 decorations.
        SourcesTestRunner
            .runActionAndWaitForExactBreakpointDecorations(
                javaScriptSourceFrame, [[2, 2]],
                () => SourcesTestRunner.createNewBreakpoint(
                    javaScriptSourceFrame, 2, '', true))
            .then(removeBreakpoint);
      }

      function removeBreakpoint() {
        TestRunner.addResult('Toggle breakpoint');
        SourcesTestRunner
            .runActionAndWaitForExactBreakpointDecorations(
                javaScriptSourceFrame, [],
                () => SourcesTestRunner.toggleBreakpoint(
                    javaScriptSourceFrame, 2))
            .then(next);
      }
    },

    function testTwoBreakpointsResolvedInOneLine(next) {
      var javaScriptSourceFrame;
      SourcesTestRunner.showScriptSource('edit-me-breakpoints.js', addBreakpoint);

      async function addBreakpoint(sourceFrame) {
        javaScriptSourceFrame = sourceFrame;
        TestRunner.addResult('Setting breakpoint');
        await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(
            javaScriptSourceFrame, [[2, 2]],
            () => SourcesTestRunner.createNewBreakpoint(
                javaScriptSourceFrame, 2, '', true));
        await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(
            javaScriptSourceFrame, [[2, 2]],
            () => SourcesTestRunner.createNewBreakpoint(
                javaScriptSourceFrame, 2, 'true', true));
        removeBreakpoint();
      }

      function removeBreakpoint() {
        TestRunner.addResult('Toggle breakpoint');
        SourcesTestRunner.removeBreakpoint(javaScriptSourceFrame, 2);
        SourcesTestRunner
            .runActionAndWaitForExactBreakpointDecorations(
                javaScriptSourceFrame, [], () => {}, true)
            .then(next);
      }
    },

    function testDecorationInGutter(next) {
      var javaScriptSourceFrame;
      SourcesTestRunner.showScriptSource('edit-me-breakpoints.js', addRegularDisabled);

      function addRegularDisabled(sourceFrame) {
        javaScriptSourceFrame = sourceFrame;
        TestRunner.addResult('Adding regular disabled breakpoint');
        SourcesTestRunner
            .runActionAndWaitForExactBreakpointDecorations(
                javaScriptSourceFrame, [[2, 1]],
                () => SourcesTestRunner.createNewBreakpoint(
                    javaScriptSourceFrame, 2, '', false))
            .then(addConditionalDisabled);
      }

      function addConditionalDisabled() {
        TestRunner.addResult('Adding conditional disabled breakpoint');
        SourcesTestRunner
            .runActionAndWaitForExactBreakpointDecorations(
                javaScriptSourceFrame, [[2, 1]],
                () => SourcesTestRunner.createNewBreakpoint(
                    javaScriptSourceFrame, 2, 'true', false))
            .then(addRegularEnabled);
      }

      function addRegularEnabled() {
        TestRunner.addResult('Adding regular enabled breakpoint');
        SourcesTestRunner
            .runActionAndWaitForExactBreakpointDecorations(
                javaScriptSourceFrame, [[2, 2]],
                () => SourcesTestRunner.createNewBreakpoint(
                    javaScriptSourceFrame, 2, '', true))
            .then(addConditionalEnabled);
      }

      function addConditionalEnabled() {
        TestRunner.addResult('Adding conditional enabled breakpoint');
        SourcesTestRunner
            .runActionAndWaitForExactBreakpointDecorations(
                javaScriptSourceFrame, [[2, 2]],
                () => SourcesTestRunner.createNewBreakpoint(
                    javaScriptSourceFrame, 2, 'true', true))
            .then(disableAll);
      }

      function disableAll() {
        TestRunner.addResult('Disable breakpoints');
        SourcesTestRunner
            .runActionAndWaitForExactBreakpointDecorations(
                javaScriptSourceFrame, [[2, 1]],
                () => SourcesTestRunner.toggleBreakpoint(
                    javaScriptSourceFrame, 2, true))
            .then(enabledAll);
      }

      function enabledAll() {
        TestRunner.addResult('Enable breakpoints');
        SourcesTestRunner
            .runActionAndWaitForExactBreakpointDecorations(
                javaScriptSourceFrame, [[2, 2]],
                () => SourcesTestRunner.toggleBreakpoint(
                    javaScriptSourceFrame, 2, true))
            .then(removeAll);
      }

      function removeAll() {
        TestRunner.addResult('Remove breakpoints');
        SourcesTestRunner
            .runActionAndWaitForExactBreakpointDecorations(
                javaScriptSourceFrame, [],
                () => SourcesTestRunner.toggleBreakpoint(
                    javaScriptSourceFrame, 2, false))
            .then(next);
      }
    }
  ]);
})();
