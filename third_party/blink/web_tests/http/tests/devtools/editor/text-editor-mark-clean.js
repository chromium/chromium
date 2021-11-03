// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`This test checks TextEditorModel.markClean/isClean methods\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  var textEditor = SourcesTestRunner.createTestEditor();
  textEditor.setText('1\n2\n3\n4');
  let changeGeneration = 0;

  TestRunner.runTestSuite([
    function testMarkiningInitialStateAsClean(next) {
      TestRunner.addResult('Initial state: clean=' + textEditor.isClean(changeGeneration));
      changeGeneration = textEditor.markClean();
      TestRunner.addResult('After marking clean: clean=' + textEditor.isClean(changeGeneration));
      textEditor.editRange(TextUtils.TextRange.createFromLocation(0, 0), 'newText');
      TestRunner.addResult('EDIT; clean=' + textEditor.isClean(changeGeneration));
      textEditor.undo();
      TestRunner.addResult('UNDO; clean=' + textEditor.isClean(changeGeneration));
      textEditor.redo();
      TestRunner.addResult('REDO; clean=' + textEditor.isClean(changeGeneration));
      textEditor.undo();
      TestRunner.addResult('UNDO; clean=' + textEditor.isClean(changeGeneration));
      textEditor.editRange(TextUtils.TextRange.createFromLocation(1, 0), 'newText2');
      TestRunner.addResult('EDIT; clean=' + textEditor.isClean(changeGeneration));
      textEditor.undo();
      TestRunner.addResult('UNDO; clean=' + textEditor.isClean(changeGeneration));
      next();
    },

    function testMiddleStateAsClean(next) {
      TestRunner.addResult('Initial state: clean=' + textEditor.isClean(changeGeneration));
      for (var i = 0; i < 3; ++i) {
        textEditor.editRange(TextUtils.TextRange.createFromLocation(i, 0), 'newText' + i);
        TestRunner.addResult('EDIT; clean=' + textEditor.isClean(changeGeneration));
      }
      changeGeneration = textEditor.markClean();
      TestRunner.addResult('After marking clean: clean=' + textEditor.isClean(changeGeneration));
      textEditor.editRange(TextUtils.TextRange.createFromLocation(3, 0), 'newText' + 3);
      TestRunner.addResult('EDIT; clean=' + textEditor.isClean(changeGeneration));
      for (var i = 0; i < 4; ++i) {
        textEditor.undo();
        TestRunner.addResult('UNDO; clean=' + textEditor.isClean(changeGeneration));
      }
      for (var i = 0; i < 4; ++i) {
        textEditor.redo();
        TestRunner.addResult('REDO; clean=' + textEditor.isClean(changeGeneration));
      }
      for (var i = 0; i < 2; ++i) {
        textEditor.undo();
        TestRunner.addResult('UNDO; clean=' + textEditor.isClean(changeGeneration));
      }
      textEditor.editRange(TextUtils.TextRange.createFromLocation(1, 0), 'foo');
      TestRunner.addResult('EDIT; clean=' + textEditor.isClean(changeGeneration));
      textEditor.undo();
      TestRunner.addResult('UNDO; clean=' + textEditor.isClean(changeGeneration));
      textEditor.undo();
      TestRunner.addResult('UNDO; clean=' + textEditor.isClean(changeGeneration));
      next();
    },
  ]);
})();
