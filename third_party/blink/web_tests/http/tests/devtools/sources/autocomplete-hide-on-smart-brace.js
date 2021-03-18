// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that suggest box gets hidden whenever a cursor jumps over smart brace.\n`);
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
    function testSummonSuggestBox(next) {
      TestRunner.addSniffer(
          TextEditor.TextEditorAutocompleteController.prototype, '_onSuggestionsShownForTest', onSuggestionsShown);

      textEditor.setText('one\n()');
      textEditor.setSelection(TextUtils.TextRange.createFromLocation(1, 1));
      SourcesTestRunner.typeIn(textEditor, 'o', function() {});

      function onSuggestionsShown() {
        TestRunner.addResult('Suggestions shown.');
        next();
      }
    },

    function testTypeSmartBrace(next) {
      TestRunner.addSniffer(
          TextEditor.TextEditorAutocompleteController.prototype, '_onSuggestionsHiddenForTest', onSuggestionsHidden);
      SourcesTestRunner.typeIn(textEditor, ')', function() {});

      function onSuggestionsHidden() {
        TestRunner.addResult('Suggestions hidden.');
        next();
      }
    },
  ];
})();
