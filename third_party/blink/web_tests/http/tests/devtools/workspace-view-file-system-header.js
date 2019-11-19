// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests workspace view file system headers\n');
  await TestRunner.loadModule('bindings_test_runner');

  const fs = new BindingsTestRunner.TestFileSystem('file:///this/is/a/test');
  await fs.reportCreatedPromise();

  await UI.viewManager.showView('workspace');
  const workspaceElement = (await UI.viewManager.view('workspace').widget()).element;

  const fsName = workspaceElement.querySelector('.file-system-name').textContent;
  const fsPath = workspaceElement.querySelector('.file-system-path').textContent;

  TestRunner.addResult(`File system name: ${fsName}`);
  TestRunner.addResult(`File system path: ${fsPath}`);

  TestRunner.completeTest();
})();
