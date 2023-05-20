// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

(async function() {
  TestRunner.addResult(`Tests object store and index entry deletion.\n`);
  await TestRunner.loadLegacyModule('console');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  function dumpObjectStore(treeElement) {
    TestRunner.addResult(`            Index: ${treeElement.title}`);
    treeElement.select();
    for (var entry of treeElement.view.entries)
      TestRunner.addResult(`                Key = ${entry.primaryKey.value}, value = ${JSON.stringify(entry.value.preview.properties)}`);
  }

  function dumpObjectStores() {
    TestRunner.addResult('Dumping ObjectStore data:');
    var idbDatabaseTreeElement = UI.panels.resources.sidebar.indexedDBListTreeElement.idbDatabaseTreeElements[0];
    for (var objectStoreTreeElement of idbDatabaseTreeElement.children()) {
      objectStoreTreeElement.select();
      TestRunner.addResult(`    Object store: ${objectStoreTreeElement.title}`);
      for (var entry of objectStoreTreeElement.view.entries)
        TestRunner.addResult(`            Key = ${entry.key.value}, value = ${JSON.stringify(entry.value.preview.properties)}`);
      for (var treeElement of objectStoreTreeElement.children())
        dumpObjectStore(treeElement);
    }
  }

  // Switch to resources panel.
  await TestRunner.showPanel('resources');
  var databaseAddedPromise = TestRunner.addSnifferPromise(Resources.IndexedDBTreeElement.prototype, 'addIndexedDB');
  await ApplicationTestRunner.createDatabaseAsync('database1');
  UI.panels.resources.sidebar.indexedDBListTreeElement.refreshIndexedDB();
  await databaseAddedPromise;
  UI.panels.resources.sidebar.indexedDBListTreeElement.expand();

  var idbDatabaseTreeElement = UI.panels.resources.sidebar.indexedDBListTreeElement.idbDatabaseTreeElements[0];
  await ApplicationTestRunner.createObjectStoreAsync('database1', 'objectStore1', 'index1');
  idbDatabaseTreeElement.refreshIndexedDB();
  await TestRunner.addSnifferPromise(Resources.IDBIndexTreeElement.prototype, 'updateTooltip');

  await ApplicationTestRunner.addIDBValueAsync('database1', 'objectStore1', 'testKey1', 'testValue');
  await ApplicationTestRunner.addIDBValueAsync('database1', 'objectStore1', 'testKey2', 'testValue');

  idbDatabaseTreeElement.refreshIndexedDB();
  await TestRunner.addSnifferPromise(Resources.IDBIndexTreeElement.prototype, 'updateTooltip');
  ApplicationTestRunner.dumpIndexedDBTree();

  var objectStoreTreeElement = idbDatabaseTreeElement.childAt(0);
  objectStoreTreeElement.select();
  await TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, 'updatedDataForTests');
  var indexTreeElement = objectStoreTreeElement.childAt(0);
  indexTreeElement.select();
  await TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, 'updatedDataForTests');
  dumpObjectStores();

  var node = objectStoreTreeElement.view.dataGrid.rootNode().children[0];
  node.select();
  objectStoreTreeElement.view.deleteButtonClicked(node);
  await TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, 'updatedDataForTests');
  await TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, 'updatedDataForTests');
  dumpObjectStores();

  node = indexTreeElement.view.dataGrid.rootNode().children[0];
  node.select();
  indexTreeElement.view.deleteButtonClicked(node);
  await TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, 'updatedDataForTests');
  await TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, 'updatedDataForTests');
  dumpObjectStores();

  TestRunner.completeTest();
})();
