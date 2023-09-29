// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as TextUtils from 'devtools/models/text_utils/text_utils.js';

(async function() {
  TestRunner.addResult(`This test checks text editor enter behaviour.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function codeSnippet() {
          return document.getElementById("codeSnippet").textContent;
      }
  `);

// clang-format off
function testFunction()
{
    var a = 100;
    var b = 200;
    var c = (a + b) / 2;
    console.log(a);
    console.log(b);
    console.log(c);
    if (a > b) {
        console.log(a);
    }
    return c;
}
// clang-format on

  var textEditor = SourcesTestRunner.createTestEditor();
  textEditor.setMimeType('text/javascript');
  textEditor.setReadOnly(false);
  textEditor.element.focus();

  TestRunner.runTestSuite([
    function testEnterInTheLineEnd(next) {
      textEditor.setText(testFunction.toString());
      var line = textEditor.line(2);
      textEditor.setSelection(TextUtils.TextRange.TextRange.createFromLocation(2, line.length));
      hitEnterDumpTextAndNext(next);
    },

    function testEnterAfterOpenCurlyBrace(next) {
      textEditor.setText(testFunction.toString());
      var line = textEditor.line(1);
      textEditor.setSelection(TextUtils.TextRange.TextRange.createFromLocation(1, line.length));
      hitEnterDumpTextAndNext(next);
    },

    function testEnterInTheMiddleOfLine(next) {
      textEditor.setText(testFunction.toString());
      var line = textEditor.line(2);
      textEditor.setSelection(TextUtils.TextRange.TextRange.createFromLocation(2, line.length / 2));
      hitEnterDumpTextAndNext(next);
    },

    function testEnterInTheBeginningOfTheLine(next) {
      textEditor.setText(testFunction.toString());
      textEditor.setSelection(TextUtils.TextRange.TextRange.createFromLocation(2, 0));
      hitEnterDumpTextAndNext(next);
    },

    function testEnterWithTheSelection(next) {
      textEditor.setText(testFunction.toString());
      textEditor.setSelection(new TextUtils.TextRange.TextRange(2, 2, 2, 4));
      hitEnterDumpTextAndNext(next);
    },

    function testEnterWithReversedSelection(next) {
      textEditor.setText(testFunction.toString());
      textEditor.setSelection(new TextUtils.TextRange.TextRange(2, 4, 2, 2));
      hitEnterDumpTextAndNext(next);
    },

    function testEnterWithTheMultiLineSelection(next) {
      textEditor.setText(testFunction.toString());
      textEditor.setSelection(new TextUtils.TextRange.TextRange(2, 0, 8, 4));
      hitEnterDumpTextAndNext(next);
    },

    function testEnterWithFullLineSelection(next) {
      textEditor.setText(testFunction.toString());
      textEditor.setSelection(new TextUtils.TextRange.TextRange(2, 0, 3, 0));
      hitEnterDumpTextAndNext(next);
    },

    function testEnterBeforeOpenBrace(next) {
      textEditor.setText(testFunction.toString());
      textEditor.setSelection(new TextUtils.TextRange.TextRange(8, 0, 8, 0));
      hitEnterDumpTextAndNext(next);
    },

    function testEnterMultiCursor(next) {
      textEditor.setText(testFunction.toString());
      SourcesTestRunner.setLineSelections(textEditor, [
        {line: 3, column: 0},
        {line: 5, column: 1},
        {line: 6, column: 2},
      ]);
      hitEnterDumpTextAndNext(next);
    }
  ]);

  function hitEnterDumpTextAndNext(next) {
    SourcesTestRunner.fakeKeyEvent(textEditor, 'Enter', null, step2);
    function step2() {
      SourcesTestRunner.dumpTextWithSelection(textEditor, true);
      next();
    }
  }
})();
