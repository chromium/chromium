// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `This test checks how text editor handles different movements: ctrl-left, ctrl-right, ctrl-shift-left, ctrl-backspace, alt-left, alt-right, alt-shift-left, alt-shift-right.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  // clang-format off
function testFunction(foo, bar)
{
    someFunctionCall(bar);
    var b = 42;
    return a === 1 ? true : false;
}

function testMyCamelMove(foo, bar)
{
    /* HelloWorld.TestSTRIng */
    
    var a = e === 2;    
{}
}
  // clang-format on
  var textEditor = SourcesTestRunner.createTestEditor();
  textEditor.setMimeType('text/javascript');
  textEditor.setReadOnly(false);
  textEditor.setText(testFunction.toString());
  textEditor.element.focus();

  TestRunner.addResult(textEditor.text());
  const wordJumpModifier = Host.isMac() ? 'altKey' : 'ctrlKey';
  const camelJumpModifier = Host.isMac() ? 'ctrlKey' : 'altKey';

  function dumpEditorSelection() {
    var selection = textEditor.selection();
    if (selection.isEmpty()) {
      var line = textEditor.line(selection.startLine);
      TestRunner.addResult(line.substring(0, selection.startColumn) + '|' + line.substring(selection.startColumn));
    } else {
      TestRunner.addResult('>>' + textEditor.text(selection.normalize()) + '<<');
    }
    return selection;
  }

  function setCursorAtBeginning() {
    textEditor.setSelection(TextUtils.TextRange.createFromLocation(0, 0));
  }

  function setCursorAtEnd() {
    var lastLine = textEditor.linesCount - 1;
    var lastColumn = textEditor.line(lastLine).length;
    textEditor.setSelection(TextUtils.TextRange.createFromLocation(lastLine, lastColumn));
  }

  function fireEventWhileSelectionChanges(eventType, modifiers, callback) {
    var oldSelection = textEditor.selection();

    function eventCallback() {
      var selection = dumpEditorSelection();
      if (selection.collapseToEnd().compareTo(oldSelection.collapseToEnd()) !== 0) {
        oldSelection = selection;
        SourcesTestRunner.fakeKeyEvent(textEditor, eventType, modifiers, eventCallback);
      } else {
        callback();
      }
    }
    SourcesTestRunner.fakeKeyEvent(textEditor, eventType, modifiers, eventCallback);
  }

  TestRunner.runTestSuite([
    function testCtrlRightArrow(next) {
      setCursorAtBeginning();
      dumpEditorSelection();
      fireEventWhileSelectionChanges('ArrowRight', [wordJumpModifier], next);
    },

    function testCtrlLeftArrow(next) {
      setCursorAtEnd();
      dumpEditorSelection();
      fireEventWhileSelectionChanges('ArrowLeft', [wordJumpModifier], next);
    },

    function testCtrlShiftRightArrow(next) {
      setCursorAtBeginning();
      dumpEditorSelection();
      fireEventWhileSelectionChanges('ArrowRight', [wordJumpModifier, 'shiftKey'], next);
    },

    function testCtrlShiftLeftArrow(next) {
      setCursorAtEnd();
      var selection = dumpEditorSelection();
      fireEventWhileSelectionChanges('ArrowLeft', [wordJumpModifier, 'shiftKey'], next);
    },

    function testCtrlBackspace(next) {
      setCursorAtEnd();
      TestRunner.addResult('===============');
      TestRunner.addResult(textEditor.text());
      function eventCallback() {
        TestRunner.addResult('===============');
        TestRunner.addResult(textEditor.text() + '<<');
        if (textEditor.text() !== '')
          SourcesTestRunner.fakeKeyEvent(textEditor, '\b', [wordJumpModifier], eventCallback);
        else
          next();
      }
      SourcesTestRunner.fakeKeyEvent(textEditor, '\b', [wordJumpModifier], eventCallback);
    },

    function testAltRight(next) {
      TestRunner.addResult('====== CAMEL CASE MOVEMENTS ======');
      textEditor.setText(testMyCamelMove.toString());
      setCursorAtBeginning();
      dumpEditorSelection();
      fireEventWhileSelectionChanges('ArrowRight', [camelJumpModifier], next);
    },

    function testAltLeft(next) {
      setCursorAtEnd();
      dumpEditorSelection();
      fireEventWhileSelectionChanges('ArrowLeft', [camelJumpModifier], next);
    },

    function testAltShiftRight(next) {
      setCursorAtBeginning();
      dumpEditorSelection();
      fireEventWhileSelectionChanges('ArrowRight', [camelJumpModifier, 'shiftKey'], next);
    },

    function testAltShiftLeft(next) {
      setCursorAtEnd();
      dumpEditorSelection();
      fireEventWhileSelectionChanges('ArrowLeft', [camelJumpModifier, 'shiftKey'], next);
    }
  ]);
})();
