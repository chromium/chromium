// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `This test verifies applied indentation whenever you hit enter in "{|}" or type in "}" while inside opened block.\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
<pre id="codeSnippet">{} {}
  {}
  {
    
    {
        
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
    function testSimpleCollapsedBlockExpanding(next) {
      textEditor.setText(codeSnippetText);
      SourcesTestRunner.setLineSelections(textEditor, [{line: 0, column: 1}]);
      SourcesTestRunner.fakeKeyEvent(textEditor, 'Enter', [], dumpAndNext(next));
    },

    function testMulticursorCollapsedBlockExpanding(next) {
      textEditor.setText(codeSnippetText);
      SourcesTestRunner.setLineSelections(
          textEditor, [{line: 0, column: 1}, {line: 0, column: 4}, {line: 1, column: 3}]);
      SourcesTestRunner.fakeKeyEvent(textEditor, 'Enter', [], dumpAndNext(next));
    },

    function testMulticursorCollapsedBlockNotExpanding(next) {
      textEditor.setText(codeSnippetText);
      SourcesTestRunner.setLineSelections(textEditor, [{line: 0, column: 1}, {line: 1, column: 2}]);
      SourcesTestRunner.fakeKeyEvent(textEditor, 'Enter', [], dumpAndNext(next));
    },

    function testSingleCursorClosingBracketIndent(next) {
      textEditor.setText(codeSnippetText);
      SourcesTestRunner.setLineSelections(textEditor, [{line: 3, column: 0}]);
      SourcesTestRunner.fakeKeyEvent(textEditor, '}', [], dumpAndNext(next));
    },

    function testMulticursorClosingBracketIndent(next) {
      textEditor.setText(codeSnippetText);
      SourcesTestRunner.setLineSelections(textEditor, [{line: 3, column: 5}, {line: 5, column: 9}]);
      SourcesTestRunner.fakeKeyEvent(textEditor, '}', [], dumpAndNext(next));
    },

    function testMulticursorClosingBracketIndentNotExecuted(next) {
      textEditor.setText(codeSnippetText);
      SourcesTestRunner.setLineSelections(textEditor, [{line: 3, column: 5}, {line: 4, column: 5}]);
      SourcesTestRunner.fakeKeyEvent(textEditor, '}', [], dumpAndNext(next));
    }
  ];
})();
