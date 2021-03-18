// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `This test checks how text editor updates autocompletion dictionary in a response to user input.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadLegacyModule('text_editor');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('debugger/resources/edit-me.js');

  SourcesTestRunner.showScriptSource('edit-me.js', onSourceFrame);

  var textEditor;
  function onSourceFrame(sourceFrame) {
    textEditor = sourceFrame.textEditor;
    textEditor.element.focus();
    TestRunner.runTestSuite(testSuite);
  }

  var testSuite = [
    // This test has to be the first. It validates that autocompletion controller
    // will initialize as a key will be pressed.
    function testCompletionsShowUpOnKeyPress(next) {
      textEditor.setText('name1 name2 name3 name4\nna');
      textEditor.setSelection(TextUtils.TextRange.createFromLocation(1, 2));
      TestRunner.addSniffer(
          TextEditor.TextEditorAutocompleteController.prototype, '_onSuggestionsShownForTest',
          onAutocompletionSuggestBox);
      SourcesTestRunner.typeIn(textEditor, 'm');
      function onAutocompletionSuggestBox() {
        document.activeElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
        dumpDictionary(next);
      }
    },

    function testSetInitialText(next) {
      textEditor.setText('one two three3_\nfour five\na_b\nsix\n123foo\n132\nseven');
      dumpDictionary(next);
    },

    function testAlphaNumericWords(next) {
      textEditor.setText('2 2foo foo2 2foo4 foo3bar');
      dumpDictionary(next);
    },

    function testRemoveDuplicate(next) {
      textEditor.setText('one\none');
      textEditor.setSelection(new TextUtils.TextRange(0, 0, 0, 3));
      SourcesTestRunner.typeIn(textEditor, '\b', dumpDictionary.bind(null, next));
    },

    function testSetText(next) {
      textEditor.setText('dog cat \'mouse\';dog bird');
      dumpDictionary(next);
    },

    function testSimpleEdit(next) {
      textEditor.setSelection(TextUtils.TextRange.createFromLocation(0, 3));
      SourcesTestRunner.typeIn(textEditor, '\b', dumpDictionary.bind(null, next));
    },

    function testDeleteOneDogAndOneCat(next) {
      textEditor.setSelection(TextUtils.TextRange.createFromLocation(0, 6));
      SourcesTestRunner.typeIn(textEditor, '\b\b\b\b\b\b', dumpDictionary.bind(null, next));
    }
  ];

  function dumpDictionary(next) {
    var wordsInDictionary = textEditor._autocompleteController._dictionary.wordsWithPrefix('');
    TestRunner.addResult('========= Text in editor =========');
    SourcesTestRunner.dumpTextWithSelection(textEditor);
    TestRunner.addResult('======= Words in dictionary =======');
    TestRunner.addResult('[' + wordsInDictionary.sort().join(', ') + ']');
    TestRunner.addResult('=============');
    next();
  }
})();
