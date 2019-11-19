// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.loadModule('bindings_test_runner');

  TestRunner.addResult(
      'Tests accessibility in the settings workspace view using the axe-core linter.');

  const fs = new BindingsTestRunner.TestFileSystem('file:///this/is/a/test');
  await fs.reportCreatedPromise();

  await UI.viewManager.showView('workspace');
  const workspaceWidget = await UI.viewManager.view('workspace').widget();

  await AxeCoreTestRunner.runValidation(workspaceWidget.element);
  TestRunner.completeTest();
})();
