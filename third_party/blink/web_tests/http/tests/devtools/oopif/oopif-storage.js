// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify DOM storage with OOPIFs`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();
  await TestRunner.navigatePromise('resources/page.html');
  await TestRunner.showPanel('resources');

  TestRunner.deprecatedRunAfterPendingDispatches(function() {
    const localStorageTree = Resources.ResourcesPanel.instance().sidebar.localStorageListTreeElement;
    localStorageTree.expandRecursively(1000);
    const sessionStorageTree = Resources.ResourcesPanel.instance().sidebar.sessionStorageListTreeElement;
    sessionStorageTree.expandRecursively(1000);

    TestRunner.addResult('Local Storage:');
    TestRunner.addResult(localStorageTree.childrenListElement.deepTextContent());

    TestRunner.addResult('Session Storage:');
    TestRunner.addResult(sessionStorageTree.childrenListElement.deepTextContent());

    TestRunner.completeTest();
  });
})();
