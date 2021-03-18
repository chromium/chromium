// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that console does not hide autocomplete during typing.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadLegacyModule('text_editor');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      window.foobar = "foobar";
      window.foobaz = "foobaz";
  `);

  ConsoleTestRunner.waitUntilConsoleEditorLoaded().then(onConsoleEditorLoaded);

  var consoleEditor;
  function onConsoleEditorLoaded(editor) {
    consoleEditor = editor;
    TestRunner.runTestSuite(testSuite);
  }

  var testSuite = [
    function testSummonSuggestBox(next) {
      TestRunner.addSniffer(
          TextEditor.TextEditorAutocompleteController.prototype, '_onSuggestionsShownForTest', onSuggestionsShown);
      SourcesTestRunner.typeIn(consoleEditor, 'f');

      function onSuggestionsShown() {
        TestRunner.addResult('Suggestions shown.');
        next();
      }
    },

    function testTypeText(next) {
      TestRunner.addSniffer(
          TextEditor.TextEditorAutocompleteController.prototype, '_onSuggestionsHiddenForTest', onSuggestionsHidden);
      TestRunner.addSniffer(
          TextEditor.TextEditorAutocompleteController.prototype, '_onCursorActivityHandledForTest',
          onCursorActivityHandled);
      SourcesTestRunner.typeIn(consoleEditor, 'o');

      var activityHandled = false;

      function onSuggestionsHidden() {
        if (activityHandled)
          return;
        TestRunner.addResult('FAIL: suggestbox is hidden during typing.');
        TestRunner.completeTest();
      }

      function onCursorActivityHandled() {
        TestRunner.addResult('SUCCESS: suggestbox is not hidden during typing.');
        activityHandled = true;
        next();
      }
    }
  ];
})();
