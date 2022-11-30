// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that fs.createFile is creating UISourceCode atomically with content`);
  await TestRunner.loadTestModule('bindings_test_runner');

  var folderLocation = '/var/test';
  await (new BindingsTestRunner.TestFileSystem(folderLocation)).reportCreatedPromise();

  Workspace.workspace.addEventListener(Workspace.Workspace.Events.UISourceCodeAdded, async event => {
    var uiSourceCode = event.data;
    var content = await uiSourceCode.requestContent();
    TestRunner.addResult('Added: ' + uiSourceCode.url());
    TestRunner.addResult('With content: ' + content.content);
    TestRunner.completeTest();
  });

  var fsWorkspaceBinding = await Workspace.workspace.project('file://' + folderLocation);
  fsWorkspaceBinding.createFile('', 'test.txt', 'file content');
})()
