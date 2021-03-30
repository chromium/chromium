// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `This test verifies editor's "Goto Matching Bracket" behavior, which is triggered via Ctrl-M shortcut.\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
<pre id="codeSnippet">function MyClass(a, b)
{
    console.log(&quot;Test&quot;);
}
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

  var testSuite = [
    function testSingleCursorFromOutsideOpenBracket(next) {
      textEditor.setText(codeSnippetText);
      SourcesTestRunner.setLineSelections(textEditor, [{line: 0, column: 16}]);
      SourcesTestRunner.fakeKeyEvent(textEditor, 'M', ['ctrlKey'], dumpAndNext(next));
    },

    function testSingleCursorFromInsideOpenBracket(next) {
      textEditor.setText(codeSnippetText);
      SourcesTestRunner.setLineSelections(textEditor, [{line: 0, column: 17}]);
      SourcesTestRunner.fakeKeyEvent(textEditor, 'M', ['ctrlKey'], dumpAndNext(next));
    },

    function testSingleCursorFromOutsideCloseBracket(next) {
      textEditor.setText(codeSnippetText);
      SourcesTestRunner.setLineSelections(textEditor, [{line: 3, column: 1}]);
      SourcesTestRunner.fakeKeyEvent(textEditor, 'M', ['ctrlKey'], dumpAndNext(next));
    },

    function testSingleCursorFromInsideCloseBracket(next) {
      textEditor.setText(codeSnippetText);
      SourcesTestRunner.setLineSelections(textEditor, [{line: 3, column: 0}]);
      SourcesTestRunner.fakeKeyEvent(textEditor, 'M', ['ctrlKey'], dumpAndNext(next));
    },

    function testMulticursor(next) {
      textEditor.setText(codeSnippetText);
      SourcesTestRunner.setLineSelections(
          textEditor, [{line: 0, column: 16}, {line: 0, column: 21}, {line: 3, column: 0}, {line: 2, column: 10}]);
      SourcesTestRunner.fakeKeyEvent(textEditor, 'M', ['ctrlKey'], dumpAndNext(next));
    },
  ];
})();
