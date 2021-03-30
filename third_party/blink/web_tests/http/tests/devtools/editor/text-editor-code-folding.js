// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`This tests the code folding setting.\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  const textEditor = SourcesTestRunner.createTestEditor(500);
  textEditor.setMimeType('text/javascript');
  const text = `function foldMe() {
  var x = 5;
  return {
    'fold': true,
    'me': true
  }
}`;
  textEditor.setText(text);
  Common.moduleSetting('textEditorCodeFolding').set(false);
  // CodeMirror does setTimeout(400) before updating folding.
  await new Promise(x => setTimeout(x, 401));
  TestRunner.addResult('With code folding off:');
  printFoldablePositions();

  TestRunner.addResult('\nWith code folding on:');
  // CodeMirror does setTimeout(400) before updating folding.
  await new Promise(x => setTimeout(x, 401));
  Common.moduleSetting('textEditorCodeFolding').set(true);
  printFoldablePositions();

  TestRunner.completeTest();

  function printFoldablePositions() {
    const foldables =
        textEditor.element.querySelectorAll('.CodeMirror-foldgutter-open');
    const lineNumbers = new Set();
    for (const foldable of foldables)
      lineNumbers.add(
          parseInt(foldable.parentElement.parentElement.textContent) - 1);
    const lines = text.split('\n');
    for (let i = 0; i < lines.length; i++) {
      if (lineNumbers.has(i))
        TestRunner.addResult('>' + lines[i]);
      else
        TestRunner.addResult(' ' + lines[i]);
    }
  }
})();
