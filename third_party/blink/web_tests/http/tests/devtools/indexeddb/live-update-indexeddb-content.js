// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as Application from 'devtools/panels/application/application.js';

(async function() {
  TestRunner.addResult(`Tests that the IndexedDB database content live updates.\n`);
  await TestRunner.navigatePromise('http://127.0.0.1:8000/devtools/indexeddb/resources/without-indexed-db.html');
  await ApplicationTestRunner.setupIndexedDBHelpers();
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  let indexedDBModel = TestRunner.mainTarget.model(Application.IndexedDBModel.IndexedDBModel);
  indexedDBModel.throttler.timeout = 0;
  var objectStore;
  var objectStoreView;
  var indexView;

  function isMarkedNeedsRefresh() {
    if (!objectStore) {
      objectStore = Application.ResourcesPanel.ResourcesPanel.instance().sidebar.indexedDBListTreeElement.idbDatabaseTreeElements[0].childAt(0);
      objectStore.onselect(false);
      objectStore.childAt(0).onselect(false);
      objectStoreView = objectStore.view;
      indexView = objectStore.childAt(0).view;
    }
    TestRunner.addResult('Object store marked needs refresh = ' + objectStoreView.needsRefresh.visible());
    TestRunner.addResult('Index marked needs refresh = ' + indexView.needsRefresh.visible());
  }

  let promise = TestRunner.addSnifferPromise(Application.ApplicationPanelSidebar.IndexedDBTreeElement.prototype, 'addIndexedDB');
  await ApplicationTestRunner.createDatabaseAsync('database1');
  await promise;
  promise = TestRunner.addSnifferPromise(Application.ApplicationPanelSidebar.IDBObjectStoreTreeElement.prototype, 'update');
  await ApplicationTestRunner.createObjectStoreAsync('database1', 'objectStore1', 'index1');
  await promise;
  ApplicationTestRunner.dumpIndexedDBTree();

  isMarkedNeedsRefresh();
  TestRunner.addResult('\nAdd entry to objectStore1:');
  promise = TestRunner.addSnifferPromise(Application.IndexedDBViews.IDBDataView.prototype, 'markNeedsRefresh');
  await ApplicationTestRunner.addIDBValueAsync('database1', 'objectStore1', 'testKey', 'testValue');
  await promise;
  isMarkedNeedsRefresh();
  ApplicationTestRunner.dumpObjectStores();

  TestRunner.addResult('\nRefresh views:');
  promise = TestRunner.addSnifferPromise(Application.IndexedDBViews.IDBDataView.prototype, 'updatedDataForTests');
  objectStoreView.updateData(true);
  await promise;
  promise = TestRunner.addSnifferPromise(Application.IndexedDBViews.IDBDataView.prototype, 'updatedDataForTests');
  indexView.updateData(true);
  await promise;
  isMarkedNeedsRefresh();
  ApplicationTestRunner.dumpObjectStores();

  TestRunner.addResult('\nDelete entry from objectStore1:');
  promise = TestRunner.addSnifferPromise(Application.IndexedDBViews.IDBDataView.prototype, 'markNeedsRefresh');
  await ApplicationTestRunner.deleteIDBValueAsync('database1', 'objectStore1', 'testKey');
  await promise;
  isMarkedNeedsRefresh();
  ApplicationTestRunner.dumpObjectStores();

  TestRunner.addResult('\nRefresh views:');
  promise = TestRunner.addSnifferPromise(Application.IndexedDBViews.IDBDataView.prototype, 'updatedDataForTests');
  objectStoreView.updateData(true);
  await promise;
  promise = TestRunner.addSnifferPromise(Application.IndexedDBViews.IDBDataView.prototype, 'updatedDataForTests');
  indexView.updateData(true);
  await promise;
  isMarkedNeedsRefresh();
  ApplicationTestRunner.dumpObjectStores();

  promise = TestRunner.addSnifferPromise(Application.ApplicationPanelSidebar.IndexedDBTreeElement.prototype, 'setExpandable');
  await ApplicationTestRunner.deleteDatabaseAsync('database1');
  await promise;
  TestRunner.completeTest();
})();
