// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that text editor's mimeType gets changed as UISourceCode gets renamed.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadTestModule('bindings_test_runner');
  await TestRunner.showPanel('sources');

  var foo_js = {content: 'console.log(\'foo.js!\');', time: null};

  var fs = new BindingsTestRunner.TestFileSystem('file:///var/www');
  BindingsTestRunner.addFiles(fs, {
    'scripts/foo.js': foo_js,
  });
  await new Promise(fulfill => fs.reportCreated(fulfill));
  TestRunner.markStep('Open foo.js editor');
  var fileUISourceCode = await TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.FileSystem);
  await dumpEditorMimeType();

  TestRunner.markStep('Rename foo.js => foo.css');
  await fileUISourceCode.rename('foo.css');
  await dumpEditorMimeType();

  TestRunner.completeTest();

  async function dumpEditorMimeType() {
    var sourceFrame = await SourcesTestRunner.showUISourceCodePromise(fileUISourceCode);
    var textEditor = sourceFrame.textEditor;
    TestRunner.addResult('Text editor mimeType: ' + textEditor.mimeType());
  }
})();
