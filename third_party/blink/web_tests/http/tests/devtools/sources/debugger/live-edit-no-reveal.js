// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as TextUtils from 'devtools/models/text_utils/text_utils.js';
import * as Sources from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(`Tests live edit feature.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/edit-me-when-paused-no-reveal.js');

  var panel = Sources.SourcesPanel.SourcesPanel.instance();
  var sourceFrame;

  function didStepInto() {
    TestRunner.addResult('Did step into');
  }

  TestRunner.addSniffer(TestRunner.debuggerModel, 'stepInto', didStepInto, true);

  function testLiveEditWhenPausedDoesNotCauseCursorMove(oldText, newText, next) {
    SourcesTestRunner.showScriptSource('edit-me-when-paused-no-reveal.js', didShowScriptSource);

    async function didShowScriptSource(sourceFrame) {
      SourcesTestRunner.waitUntilPaused(paused);
      await SourcesTestRunner.setBreakpoint(sourceFrame, 8, '', true);
      TestRunner.evaluateInPage('f1()', didEvaluateInPage);
    }

    function paused(callFrames) {
      sourceFrame = panel.visibleView;
      SourcesTestRunner.removeBreakpoint(sourceFrame, 8);
      TestRunner.addSniffer(TestRunner.debuggerModel, '_didEditScriptSource', didEditScriptSource);
      panel._updateLastModificationTimeForTest();
      SourcesTestRunner.replaceInSource(sourceFrame, oldText, newText);
      TestRunner.addResult('Moving cursor to (0, 0).');
      sourceFrame.setSelection(TextUtils.TextRange.TextRange.createFromLocation(0, 0));
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

    async function didShowScriptSource(sourceFrame) {
      SourcesTestRunner.waitUntilPaused(paused);
      await SourcesTestRunner.setBreakpoint(sourceFrame, 8, '', true);
      TestRunner.evaluateInPage('f1()', didEvaluateInPage);
    }

    function paused(callFrames) {
      sourceFrame = panel.visibleView;
      SourcesTestRunner.removeBreakpoint(sourceFrame, 8);
      TestRunner.addSniffer(TestRunner.debuggerModel, '_didEditScriptSource', didEditScriptSource);
      panel._lastModificationTimeoutPassedForTest();
      SourcesTestRunner.replaceInSource(sourceFrame, oldText, newText);
      TestRunner.addResult('Moving cursor to (0, 0).');
      sourceFrame.setSelection(TextUtils.TextRange.TextRange.createFromLocation(0, 0));
      TestRunner.addResult('Committing live edit.');
      SourcesTestRunner.commitSource(sourceFrame);
    }

    function didEditScriptSource() {
      TestRunner.addResult('Stepping into...');
      TestRunner.addSniffer(Sources.SourcesView.SourcesView.prototype, 'showSourceLocation', didRevealAfterStepInto);
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
