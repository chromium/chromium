// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that jumping to previous location works as intended.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/example-fileset-for-test.js');
  await TestRunner.addScriptTag('resources/jump-text.js');

  var panel = UI.panels.sources;
  var sourcesView = panel._sourcesView;
  var historyManager = sourcesView._historyManager;
  var editorContainer = sourcesView._editorContainer;

  function rollback() {
    historyManager.rollback();
  }

  function rollover() {
    historyManager.rollover();
  }

  function dumpSelection(label) {
    var selection = panel.visibleView.textEditor.selection();
    var label = '<' + label + '>';
    var fileName = '[' + panel.visibleView.uiSourceCode().url().split('/').pop() + ']';
    var selectionText = '';
    if (selection.isEmpty())
      selectionText = 'line: ' + selection.startLine + ' column: ' + selection.startColumn;
    else
      selectionText = '(NOT EMPTY): ' + selection.toString();
    TestRunner.addResult(label + ' ' + selectionText + ' ' + fileName);
  }

  function eventSenderClickAtEditor(editor, lineNumber, columnNumber) {
    editor.scrollToLine(lineNumber);
    var coordinates = editor.cursorPositionToCoordinates(lineNumber, columnNumber);
    eventSender.mouseMoveTo(coordinates.x, coordinates.y);
    eventSender.mouseDown();
    eventSender.mouseUp();
    dumpSelection('Mouse click (' + lineNumber + ', ' + columnNumber + ')');
  }

  function clickAndDump(editor, lines, columns) {
    for (var i = 0; i < lines.length; ++i) {
      var lineNumber = lines[i];
      var columnNumber = columns[i];
      var originSelection = editor.selection();
      editor.setSelection(TextUtils.TextRange.createFromLocation(lineNumber, columnNumber));
      editor._reportJump(originSelection, editor.selection());
      dumpSelection('Mouse click (' + lineNumber + ', ' + columnNumber + ')');
    }
  }

  TestRunner.runTestSuite([
    function testSimpleMovements(next) {
      SourcesTestRunner.showScriptSource('example-fileset-for-test.js', onContentLoaded);

      function onContentLoaded() {
        var editor = panel.visibleView.textEditor;
        dumpSelection('Initial position');
        eventSenderClickAtEditor(editor, 4, 7, true);

        SourcesTestRunner.typeIn(editor, '\nSome more text here', step2);
      }

      function step2() {
        var editor = panel.visibleView.textEditor;
        dumpSelection('Typed in some text');

        rollback();
        dumpSelection('Rolled back');
        SourcesTestRunner.typeIn(editor, '\nSome more text here as well\n', step3);
      }

      function step3() {
        var editor = panel.visibleView.textEditor;
        dumpSelection('Typed in some text');

        rollover();
        dumpSelection('Rolled over');
        next();
      }
    },

    function testSequentialJumps(next) {
      var editor = panel.visibleView.textEditor;
      // Hide inspector view to significantly speed up following tests from testsuite.
      TestRunner.hideInspectorView();
      const jumpsToDo = 4;
      clickAndDump(editor, [10, 11, 12, 13], [3, 4, 5, 6]);

      for (var i = 0; i < jumpsToDo; ++i) {
        rollback();
        dumpSelection('Rolled back');
      }
      for (var i = 0; i < jumpsToDo; ++i) {
        rollover();
        dumpSelection('Rolled over');
      }
      next();
    },

    function testDeletePreviousJumpLocations(next) {
      var editor = panel.visibleView.textEditor;
      editor.editRange(new TextUtils.TextRange(9, 0, 15, 0), '');
      dumpSelection('Removed lines from 9 to 15');
      rollback();
      dumpSelection('Rolled back');
      rollover();
      dumpSelection('Rolled over');
      next();
    },

    function testDeleteNextJumpLocations(next) {
      var editor = panel.visibleView.textEditor;
      const jumpsToDo = 4;
      clickAndDump(editor, [10, 11, 12, 13], [3, 4, 5, 6]);

      for (var i = 0; i < jumpsToDo; ++i)
        rollback();
      dumpSelection('Rolled back 4 times');
      editor.editRange(new TextUtils.TextRange(9, 0, 11, 0), '');
      dumpSelection('Removed lines from 9 to 11');
      rollover();
      dumpSelection('Rolled over');
      next();
    },

    function testCrossFileJump(next) {
      SourcesTestRunner.showScriptSource('jump-text.js', onContentLoaded);
      function onContentLoaded() {
        var editor = panel.visibleView.textEditor;
        dumpSelection('Opened jump-text.js');
        clickAndDump(editor, [10, 11], [1, 1]);
        for (var i = 0; i < 4; ++i) {
          rollback();
          dumpSelection('Rolled back');
        }
        for (var i = 0; i < 4; ++i) {
          rollover();
          dumpSelection('Rolled over');
        }
        next();
      }
    },

    function testCloseCrossFile(next) {
      var selectedTab = editorContainer._tabbedPane.selectedTabId;
      editorContainer._tabbedPane.closeTab(selectedTab);
      dumpSelection('Close active tab');
      for (var i = 0; i < 1; ++i) {
        rollback();
        dumpSelection('Rolled back');
      }
      for (var i = 0; i < 3; ++i) {
        rollover();
        dumpSelection('Rolled over');
      }
      next();
    },

    function testHistoryDepth(next) {
      var lines = [];
      var columns = [];
      const jumpsAmount = Sources.EditingLocationHistoryManager.HistoryDepth;
      for (var i = 0; i < jumpsAmount; ++i) {
        lines.push(i + 10);
        columns.push(7);
      }
      var editor = panel.visibleView.textEditor;
      clickAndDump(editor, lines, columns);
      for (var i = 0; i < jumpsAmount; ++i) {
        rollback();
        dumpSelection('Rolled back');
      }
      next();
    },

    function testInFileSearch(next) {
      var searchableView = panel.searchableView();

      dumpSelection('Before searching');

      searchableView.showSearchField();
      searchableView._searchInputElement.value = 'generate_';
      searchableView._performSearch(true, true);
      for (var i = 0; i < 3; ++i)
        searchableView.handleFindNextShortcut();
      searchableView.handleCancelSearchShortcut();

      dumpSelection('After searching');
      rollback();
      dumpSelection('Rolled back');
      rollover();
      dumpSelection('Rolled over');
      next();
    },

    function testShowAnchorLocation(next) {
      dumpSelection('Before switching to other panel');
      SourcesTestRunner.waitForScriptSource('jump-text.js', onScriptSource);
      function onScriptSource(uiSourceCode) {
        var linkifier = new Components.Linkifier();
        var anchorURI = uiSourceCode.url();
        var anchor = linkifier.linkifyScriptLocation(SDK.targetManager.mainTarget(), null, anchorURI, 10, {columnNumber: 1});
        var info = Components.Linkifier.linkInfo(anchor);
        Common.Revealer.reveal(info.uiLocation).then(function() {
          TestRunner.addResult('Selection: ' + panel.visibleView.textEditor.selection().toString());
          dumpSelection('Showed anchor in ' + anchorURI.split('/').pop() + ' with line 333 column 3');
          rollback();
          dumpSelection('Rolled back');
          rollover();
          dumpSelection('Rolled over');
          next();
        });
      }
    },

    function testShowUISourceCode(next) {
      dumpSelection('Initial state');

      SourcesTestRunner.waitForScriptSource('example-fileset-for-test.js', onScriptSourceLoaded);
      function onScriptSourceLoaded(uiSourceCode) {
        var jumps = [20, 10, 30];
        for (var i = 0; i < jumps.length; ++i) {
          panel.showUISourceCode(uiSourceCode, jumps[i]);
          dumpSelection('jump to line ' + jumps[i]);
        }
        panel.showUISourceCode(uiSourceCode, 40, 10);
        dumpSelection('highlight line 40');
        for (var i = 0; i < jumps.length + 1; ++i) {
          rollback();
          dumpSelection('rollback');
        }
        next();
      }
    }
  ]);
})();
