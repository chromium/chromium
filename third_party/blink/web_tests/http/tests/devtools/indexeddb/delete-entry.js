// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests object store and index entry deletion.\n`);
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  function dumpObjectStore(treeElement) {
    TestRunner.addResult(`            Index: ${treeElement.title}`);
    treeElement.select();
    for (var entry of treeElement._view._entries)
      TestRunner.addResult(`                Key = ${entry.primaryKey.value}, value = ${JSON.stringify(entry.value.preview.properties)}`);
  }

  function dumpObjectStores() {
    TestRunner.addResult('Dumping ObjectStore data:');
    var idbDatabaseTreeElement = UI.panels.resources._sidebar.indexedDBListTreeElement._idbDatabaseTreeElements[0];
    for (var objectStoreTreeElement of idbDatabaseTreeElement.children()) {
      objectStoreTreeElement.select();
      TestRunner.addResult(`    Object store: ${objectStoreTreeElement.title}`);
      for (var entry of objectStoreTreeElement._view._entries)
        TestRunner.addResult(`            Key = ${entry.key.value}, value = ${JSON.stringify(entry.value.preview.properties)}`);
      for (var treeElement of objectStoreTreeElement.children())
        dumpObjectStore(treeElement);
    }
  }

  // Switch to resources panel.
  await TestRunner.showPanel('resources');
  var databaseAddedPromise = TestRunner.addSnifferPromise(Resources.IndexedDBTreeElement.prototype, '_addIndexedDB');
  await ApplicationTestRunner.createDatabaseAsync('database1');
  UI.panels.resources._sidebar.indexedDBListTreeElement.refreshIndexedDB();
  await databaseAddedPromise;
  UI.panels.resources._sidebar.indexedDBListTreeElement.expand();

  var idbDatabaseTreeElement = UI.panels.resources._sidebar.indexedDBListTreeElement._idbDatabaseTreeElements[0];
  await ApplicationTestRunner.createObjectStoreAsync('database1', 'objectStore1', 'index1');
  idbDatabaseTreeElement._refreshIndexedDB();
  await TestRunner.addSnifferPromise(Resources.IDBIndexTreeElement.prototype, '_updateTooltip');

  await ApplicationTestRunner.addIDBValueAsync('database1', 'objectStore1', 'testKey1', 'testValue');
  await ApplicationTestRunner.addIDBValueAsync('database1', 'objectStore1', 'testKey2', 'testValue');

  idbDatabaseTreeElement._refreshIndexedDB();
  await TestRunner.addSnifferPromise(Resources.IDBIndexTreeElement.prototype, '_updateTooltip');
  ApplicationTestRunner.dumpIndexedDBTree();

  var objectStoreTreeElement = idbDatabaseTreeElement.childAt(0);
  objectStoreTreeElement.select();
  await TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, '_updatedDataForTests');
  var indexTreeElement = objectStoreTreeElement.childAt(0);
  indexTreeElement.select();
  await TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, '_updatedDataForTests');
  dumpObjectStores();

  var node = objectStoreTreeElement._view._dataGrid.rootNode().children[0];
  node.select();
  objectStoreTreeElement._view._deleteButtonClicked(node);
  await TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, '_updatedDataForTests');
  await TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, '_updatedDataForTests');
  dumpObjectStores();

  node = indexTreeElement._view._dataGrid.rootNode().children[0];
  node.select();
  indexTreeElement._view._deleteButtonClicked(node);
  await TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, '_updatedDataForTests');
  await TestRunner.addSnifferPromise(Resources.IDBDataView.prototype, '_updatedDataForTests');
  dumpObjectStores();

  TestRunner.completeTest();
})();
