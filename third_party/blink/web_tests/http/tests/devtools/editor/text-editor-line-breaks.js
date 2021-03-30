// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `This test checks that line endings are inferred from the initial text content, not incremental editing.\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  TestRunner.runTestSuite([
    function testCRInitial(next) {
      var textEditor = SourcesTestRunner.createTestEditor();
      textEditor.setText('1\n2\n3\n');
      TestRunner.addResult(encodeURI(textEditor.text()));
      next();
    },

    function testCRLFInitial(next) {
      var textEditor = SourcesTestRunner.createTestEditor();
      textEditor.setText('1\r\n2\r\n3\r\n');
      TestRunner.addResult(encodeURI(textEditor.text()));
      next();
    },

    function testCREdit(next) {
      var textEditor = SourcesTestRunner.createTestEditor();
      textEditor.setText('1\n2\n3\n');
      textEditor.editRange(new TextUtils.TextRange(1, 0, 1, 0), 'foo\r\nbar');
      TestRunner.addResult(encodeURI(textEditor.text()));
      next();
    },

    function testCRLFEdit(next) {
      var textEditor = SourcesTestRunner.createTestEditor();
      textEditor.setText('1\r\n2\r\n3\r\n');
      textEditor.editRange(new TextUtils.TextRange(1, 0, 1, 0), 'foo\r\nbar');
      TestRunner.addResult(encodeURI(textEditor.text()));
      next();
    }
  ]);
})();
