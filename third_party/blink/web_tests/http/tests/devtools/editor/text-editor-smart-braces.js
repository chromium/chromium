// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`This test checks text editor smart braces functionality.\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  var textEditor = SourcesTestRunner.createTestEditor();
  textEditor.setMimeType('text/javascript');
  textEditor.setReadOnly(false);
  textEditor.element.focus();

  function clearEditor() {
    textEditor.setText('');
    textEditor.setSelection(TextUtils.TextRange.createFromLocation(0, 0));
  }

  TestRunner.runTestSuite([
    function testTypeBraceSequence(next) {
      clearEditor();
      SourcesTestRunner.typeIn(textEditor, '({[', onTypedIn);
      function onTypedIn() {
        SourcesTestRunner.dumpTextWithSelection(textEditor);
        next();
      }
    },

    function testBraceOverride(next) {
      clearEditor();
      SourcesTestRunner.typeIn(textEditor, '({[]})', onTypedIn);
      function onTypedIn() {
        SourcesTestRunner.dumpTextWithSelection(textEditor);
        next();
      }
    },

    function testQuotesToCloseStringLiterals(next) {
      textEditor.setText('\'Hello');
      textEditor.setSelection(TextUtils.TextRange.createFromLocation(0, 6));
      SourcesTestRunner.typeIn(textEditor, '"\'', onTypedIn);
      function onTypedIn() {
        SourcesTestRunner.dumpTextWithSelection(textEditor);
        next();
      }
    },

    function testQuotesToCloseStringLiteralInsideLine(next) {
      textEditor.setText('console.log("information");');
      textEditor.setSelection(TextUtils.TextRange.createFromLocation(0, 24));
      SourcesTestRunner.typeIn(textEditor, '"', onTypedIn);
      function onTypedIn() {
        SourcesTestRunner.dumpTextWithSelection(textEditor);
        next();
      }
    }
  ]);
})();
