// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`The test verifies autocomplete suggestions for CSS file.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadLegacyModule('text_editor');
  await TestRunner.showPanel('sources');
  await TestRunner.addStylesheetTag('./resources/empty.css');

  SourcesTestRunner.showScriptSource('empty.css', onSourceFrame);

  var textEditor;
  var dumpSuggestions;
  function onSourceFrame(sourceFrame) {
    textEditor = sourceFrame.textEditor;
    dumpSuggestions = SourcesTestRunner.dumpSuggestions.bind(SourcesTestRunner, textEditor);
    TestRunner.runTestSuite(testSuite);
  }

  var testSuite = [
    function testClassNameAutocomplete(next) {
      dumpSuggestions(['.red { color: red }', '.blue { color: blue }', '.|']).then(next);
    },

    function testPropertyNameAutocomplete1(next) {
      dumpSuggestions(['.red { color: red }', '.blue { c|']).then(next);
    },

    function testPropertyNameAutocomplete2(next) {
      dumpSuggestions([
        '.my-class { -|webkit-border: 1px solid black; -webkit-color: blue;', 'text-align: }'
      ]).then(next);
    },

    function testPropertyValueAutocomplete1(next) {
      dumpSuggestions(['.red { border-style: |', '/* some other words to mess up */']).then(next);
    },

    function testPropertyValueAutocomplete2(next) {
      dumpSuggestions(['.red { border-style: d|', '/* dial drummer dig */']).then(next);
    },

    function testPropertyValueAutocomplete3(next) {
      dumpSuggestions(['.red { border-style: z|', '/* zipper zorro zion */']).then(next);
    },

    function testPropertyValueAutocomplete4(next) {
      dumpSuggestions(['.red { border-style/* comment */: /* comment */|']).then(next);
    },

    function testPropertyValueAutocomplete5(next) {
      dumpSuggestions([
        '.my-class { -webkit-border: 1px solid black; -webkit-color: blue;', 'text-align: |}'
      ]).then(next);
    },

    function verifySuggestionsOnColumnTypedIn(next) {
      textEditor.element.focus();
      textEditor.setText(['.green {', '   display'].join('\n'));
      textEditor.setSelection(TextUtils.TextRange.createFromLocation(1, 10));
      SourcesTestRunner.dumpTextWithSelection(textEditor);
      TestRunner.addSniffer(
          TextEditor.TextEditorAutocompleteController.prototype, '_onSuggestionsShownForTest', suggestionsShown);
      SourcesTestRunner.typeIn(textEditor, ':');

      function suggestionsShown(words) {
        TestRunner.addResult('Suggestions displayed on \':\' symbol typed in:');
        TestRunner.addResult('[' + words.map(item => item.text).join(', ') + ']');
        next();
      }
    },
  ];
})();
