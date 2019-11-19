// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function() {
  TestRunner.addResult(`Tests accessibility in the Clear Storage view using the axe-core linter.`);
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.loadModule('axe_core_test_runner');
  await ApplicationTestRunner.resetState();
  await TestRunner.showPanel('resources');
  async function writeArray() {
    const array = Array(20000).fill(0);
    const mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;
    await new Promise(resolve => ApplicationTestRunner.createDatabase(mainFrameId, 'Database1', resolve));
    await new Promise(
        resolve => ApplicationTestRunner.createObjectStore(mainFrameId, 'Database1', 'Store1', 'id', true, resolve));
    await new Promise(
        resolve =>
            ApplicationTestRunner.addIDBValue(mainFrameId, 'Database1', 'Store1', {key: 1, value: array}, '', resolve));
  }
  async function runAxe(element) {
    try {
      const results = await axe.run(element);
      const violations = AxeCoreTestRunner.processAxeResult(results.violations);
      TestRunner.addResult(`aXe violations: ${violations}`);
    } catch (e) {
      TestRunner.addResult(`aXe threw an error: '${e}'`);
    }
  }
  await UI.viewManager.showView('resources');
  const parent = UI.panels.resources._sidebar._applicationTreeElement;
  const clearStorageElement = parent.children().find(child => child.title === ls`Clear storage`);
  clearStorageElement.select();
  const clearStorageView = UI.panels.resources.visibleView;
  TestRunner.addResult('Clear storage view is visible: ' + (clearStorageView instanceof Resources.ClearStorageView));
  await writeArray();
  await runAxe(clearStorageView.contentElement);
  TestRunner.completeTest();
})();
