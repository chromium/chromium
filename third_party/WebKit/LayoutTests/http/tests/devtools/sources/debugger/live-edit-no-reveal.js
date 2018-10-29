// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests live edit feature.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/edit-me-when-paused-no-reveal.js');

  var panel = UI.panels.sources;
  var sourceFrame;

  function didStepInto() {
    TestRunner.addResult('Did step into');
  }

  TestRunner.addSniffer(TestRunner.debuggerModel, 'stepInto', didStepInto, true);

  function testLiveEditWhenPausedDoesNotCauseCursorMove(oldText, newText, next) {
    SourcesTestRunner.showScriptSource('edit-me-when-paused-no-reveal.js', didShowScriptSource);

    function didShowScriptSource(sourceFrame) {
      SourcesTestRunner.waitUntilPaused(paused);
      SourcesTestRunner.setBreakpoint(sourceFrame, 8, '', true);
      TestRunner.evaluateInPage('f1()', didEvaluateInPage);
    }

    function paused(callFrames) {
      sourceFrame = panel.visibleView;
      SourcesTestRunner.removeBreakpoint(sourceFrame, 8);
      TestRunner.addSniffer(TestRunner.debuggerModel, '_didEditScriptSource', didEditScriptSource);
      panel._updateLastModificationTimeForTest();
      SourcesTestRunner.replaceInSource(sourceFrame, oldText, newText);
      TestRunner.addResult('Moving cursor to (0, 0).');
      sourceFrame.setSelection(TextUtils.TextRange.createFromLocation(0, 0));
      TestRunner.addResult('Committing live edit.');
      SourcesTestRunner.commitSource(sourceFrame);
    }

    function didEditScriptSource() {
      SourcesTestRunner.resumeExecution();
    }

    function didEvaluateInPage(result) {
      panel._lastModificationTimeoutPassedForTest();
      var selection = sourceFrame.textEditor.selection();
      TestRunner.addResult('Cursor position is: (' + selection.startLine + ', ' + selection.startColumn + ').');
      TestRunner.assertEquals(sourceFrame, panel.visibleView, 'Another file editor is open.');
      next();
    }
  }

  function testLiveEditWhenPausedThenStepIntoCausesCursorMove(oldText, newText, next) {
    SourcesTestRunner.showScriptSource('edit-me-when-paused-no-reveal.js', didShowScriptSource);

    function didShowScriptSource(sourceFrame) {
      SourcesTestRunner.waitUntilPaused(paused);
      SourcesTestRunner.setBreakpoint(sourceFrame, 8, '', true);
      TestRunner.evaluateInPage('f1()', didEvaluateInPage);
    }

    function paused(callFrames) {
      sourceFrame = panel.visibleView;
      SourcesTestRunner.removeBreakpoint(sourceFrame, 8);
      TestRunner.addSniffer(TestRunner.debuggerModel, '_didEditScriptSource', didEditScriptSource);
      panel._lastModificationTimeoutPassedForTest();
      SourcesTestRunner.replaceInSource(sourceFrame, oldText, newText);
      TestRunner.addResult('Moving cursor to (0, 0).');
      sourceFrame.setSelection(TextUtils.TextRange.createFromLocation(0, 0));
      TestRunner.addResult('Committing live edit.');
      SourcesTestRunner.commitSource(sourceFrame);
    }

    function didEditScriptSource() {
      TestRunner.addResult('Stepping into...');
      TestRunner.addSniffer(Sources.SourcesView.prototype, 'showSourceLocation', didRevealAfterStepInto);
      panel._lastModificationTimeoutPassedForTest();
      SourcesTestRunner.stepInto();
    }

    function didRevealAfterStepInto() {
      SourcesTestRunner.resumeExecution();
    }

    function didEvaluateInPage(result) {
      var selection = sourceFrame.textEditor.selection();
      TestRunner.addResult('Cursor position is: (' + selection.startLine + ', ' + selection.startColumn + ').');
      TestRunner.assertEquals(sourceFrame, panel.visibleView, 'Another file editor is open.');
      next();
    }
  }

  SourcesTestRunner.runDebuggerTestSuite([
    function testLiveEditWithoutStepInWhenPausedThenStepIntoCausesCursorMove(next) {
      testLiveEditWhenPausedThenStepIntoCausesCursorMove('function f2()', ' function f2()', next);
    },

    function testLiveEditWithStepInWhenPausedThenStepIntoCausesCursorMove(next) {
      testLiveEditWhenPausedThenStepIntoCausesCursorMove('return x + f2();', 'return x + f2(); ', next);
    },

    function testLiveEditWithoutStepInWhenPausedDoesNotCauseCursorMove(next) {
      testLiveEditWhenPausedDoesNotCauseCursorMove('function f2()', ' function f2()', next);
    },

    function testLiveEditWithStepInWhenPausedDoesNotCauseCursorMove(next) {
      testLiveEditWhenPausedDoesNotCauseCursorMove('return x + f2();', 'return x + f2(); ', next);
    }
  ]);
})();
