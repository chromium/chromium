// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`This test checks that text editor's revealLine centers line where needed.\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  var out = TestRunner.addResult;
  var textEditor = SourcesTestRunner.createTestEditor(500);
  textEditor.setMimeType('text/javascript');
  textEditor.setReadOnly(false);
  textEditor.element.focus();

  textEditor.setText(new Array(10000).join('\n'));

  testLineReveal(0);
  testLineReveal(500);
  testLineReveal(510);
  testLineReveal(490);
  testLineReveal(1000);
  testLineReveal(100);
  testLineReveal(9998);
  testLineReveal(-100);
  testLineReveal(textEditor.linesCount);
  testLineReveal(-1);
  testLineReveal(10100);

  function testLineReveal(lineNumber) {
    textEditor.revealPosition(lineNumber);
    var firstLine = textEditor.firstVisibleLine();
    var lastLine = textEditor.lastVisibleLine();
    var lineCentered = Math.abs(2 * lineNumber - firstLine - lastLine) <= 1;
    out('======= Revealing line: ' + lineNumber);
    out('      is line centered: ' + lineCentered);
    out('\n');
  }
  TestRunner.completeTest();
})();
