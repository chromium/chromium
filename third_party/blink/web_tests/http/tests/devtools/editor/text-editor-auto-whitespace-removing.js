// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`This test verifies that auto-appended spaces are removed on consequent enters.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
<pre id="codeSnippet">function (){}
    if (a == b) {
</pre>
  `);
  await TestRunner.dumpInspectedPageElementText('#codeSnippet');
  await TestRunner.evaluateInPagePromise(`
      function codeSnippet() {
          return document.getElementById("codeSnippet").textContent;
      }
  `);

  var textEditor = SourcesTestRunner.createTestEditor();
  textEditor.setMimeType('text/javascript');
  textEditor.setReadOnly(false);
  textEditor.element.focus();
  TestRunner.evaluateInPage('codeSnippet();', onCodeSnippet);
  var codeSnippetText;

  function onCodeSnippet(result) {
    codeSnippetText = result;
    TestRunner.runTestSuite(testSuite);
  }

  function dumpAndNext(next) {
    function innerDumpAndNext() {
      SourcesTestRunner.dumpTextWithSelection(textEditor, true);
      next();
    }
    return innerDumpAndNext;
  }

  function doubleEnter(next) {
    function onFirstEnter() {
      SourcesTestRunner.fakeKeyEvent(textEditor, 'Enter', [], dumpAndNext(next));
    }

    SourcesTestRunner.fakeKeyEvent(textEditor, 'Enter', [], onFirstEnter);
  }

  var testSuite = [
    function testCollapsedBlock(next) {
      textEditor.setText(codeSnippetText);
      SourcesTestRunner.setLineSelections(textEditor, [{line: 0, column: 12}]);
      doubleEnter(next);
    },

    function testOpenCurlyBrace(next) {
      textEditor.setText(codeSnippetText);
      SourcesTestRunner.setLineSelections(textEditor, [{line: 1, column: 17}]);
      doubleEnter(next);
    },

    function testSmartIndent(next) {
      textEditor.setText(codeSnippetText);
      SourcesTestRunner.setLineSelections(textEditor, [{line: 1, column: 2}]);
      doubleEnter(next);
    },

    function testMultiCursorSelection(next) {
      textEditor.setText(codeSnippetText);
      SourcesTestRunner.setLineSelections(textEditor, [{line: 1, column: 2}, {line: 1, column: 4}]);
      doubleEnter(next);
    },

    function testEditedAutoIndent(next) {
      textEditor.setText(codeSnippetText);
      SourcesTestRunner.setLineSelections(textEditor, [{line: 1, column: 17}]);
      SourcesTestRunner.fakeKeyEvent(textEditor, 'Enter', [], onEnter);

      function onEnter() {
        SourcesTestRunner.fakeKeyEvent(textEditor, 'W', [], onEditedText);
      }

      function onEditedText() {
        SourcesTestRunner.fakeKeyEvent(textEditor, 'Enter', [], dumpAndNext(next));
      }
    },
  ];
})();
