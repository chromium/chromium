// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the IndexedDB database content live updates.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  let indexedDBModel = TestRunner.mainTarget.model(Resources.IndexedDBModel);
  indexedDBModel.throttler.timeout = 0;
  var objectStore;
  var objectStoreView;
  var indexView;

  function isMarkedNeedsRefresh() {
    if (!objectStore) {
      objectStore = UI.panels.resources.sidebar.indexedDBListTreeElement.idbDatabaseTreeElements[0].childAt(0);
      objectStore.onselect(false);
      objectStore.childAt(0).onselect(false);
      objectStoreView = objectStore.view;
      indexView = objectStore.childAt(0).view;
    }
    TestRunner.addResult('Object store marked needs refresh = ' + objectStoreView.needsRefresh.visible());
    TestRunner.addResult('Index marked needs refresh = ' + indexView.needsRefresh.visible());
  }

  let promise = TestRunner.addSnifferPromise(Resources.IndexedDBTreeElement.prototype, 'addIndexedDB');
  await ApplicationTestRunner.createDatabaseAsync('database1');
  await promise;
  promise = TestRunner.addSnifferPromise(Resources.IDBObjectStoreTreeElement.prototype, 'update');
  await ApplicationTestRunner.createObjectStoreAsync('database1', 'objectStore1', 'index1');
  await promise;
  ApplicationTestRunner.dumpIndexedDBTree();

  isMarkedNeedsRefresh();
  TestRunner.addResult('\nAdd entry to objectStore1:');
  promise = TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, 'markNeedsRefresh');
  await ApplicationTestRunner.addIDBValueAsync('database1', 'objectStore1', 'testKey', 'testValue');
  await promise;
  isMarkedNeedsRefresh();
  ApplicationTestRunner.dumpObjectStores();

  TestRunner.addResult('\nRefresh views:');
  promise = TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, 'updatedDataForTests');
  objectStoreView.updateData(true);
  await promise;
  promise = TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, 'updatedDataForTests');
  indexView.updateData(true);
  await promise;
  isMarkedNeedsRefresh();
  ApplicationTestRunner.dumpObjectStores();

  TestRunner.addResult('\nDelete entry from objectStore1:');
  promise = TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, 'markNeedsRefresh');
  await ApplicationTestRunner.deleteIDBValueAsync('database1', 'objectStore1', 'testKey');
  await promise;
  isMarkedNeedsRefresh();
  ApplicationTestRunner.dumpObjectStores();

  TestRunner.addResult('\nRefresh views:');
  promise = TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, 'updatedDataForTests');
  objectStoreView.updateData(true);
  await promise;
  promise = TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, 'updatedDataForTests');
  indexView.updateData(true);
  await promise;
  isMarkedNeedsRefresh();
  ApplicationTestRunner.dumpObjectStores();

  promise = TestRunner.addSnifferPromise(Resources.IndexedDBTreeElement.prototype, 'setExpandable');
  await ApplicationTestRunner.deleteDatabaseAsync('database1');
  await promise;
  TestRunner.completeTest();
})();
