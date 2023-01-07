// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests refreshing the database information and data views.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('resources');

  var databaseName = 'testDatabase';
  var objectStoreName1 = 'testObjectStore1';
  var objectStoreName2 = 'testObjectStore2';
  var indexName = 'testIndex';
  var keyPath = 'testKey';

  var indexedDBModel = TestRunner.mainTarget.model(Resources.IndexedDBModel);
  indexedDBModel._throttler._timeout = 100000;  // Disable live updating.
  var databaseId;

  function waitRefreshDatabase() {
    var view = UI.panels.resources._sidebar.indexedDBListTreeElement._idbDatabaseTreeElements[0]._view;
    view._refreshDatabaseButtonClicked();
    return new Promise((resolve) => {
      TestRunner.addSniffer(Resources.IDBDatabaseView.prototype, '_updatedForTests', resolve, false);
    });
  }

  function waitRefreshDatabaseRightClick() {
    idbDatabaseTreeElement._refreshIndexedDB();
    return waitUpdateDataView();
  }

  function waitUpdateDataView() {
    return new Promise((resolve) => {
      TestRunner.addSniffer(Resources.IDBDataView.prototype, '_updatedDataForTests', resolve, false);
    });
  }

  function waitDatabaseLoaded(callback) {
    var event = indexedDBModel.addEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, () => {
      Common.EventTarget.removeEventListeners([event]);
      callback();
    });
  }

  function waitDatabaseAdded(callback) {
    var event = indexedDBModel.addEventListener(Resources.IndexedDBModel.Events.DatabaseAdded, () => {
      Common.EventTarget.removeEventListeners([event]);
      callback();
    });
    UI.panels.resources._sidebar.indexedDBListTreeElement.refreshIndexedDB();
  }

  // Initial tree
  ApplicationTestRunner.dumpIndexedDBTree();

  // Create database
  await ApplicationTestRunner.createDatabaseAsync(databaseName);
  await new Promise(waitDatabaseAdded);
  var idbDatabaseTreeElement = UI.panels.resources._sidebar.indexedDBListTreeElement._idbDatabaseTreeElements[0];
  databaseId = idbDatabaseTreeElement._databaseId;
  TestRunner.addResult('Created database.');
  ApplicationTestRunner.dumpIndexedDBTree();

  // Load indexedDb database view
  indexedDBModel.refreshDatabase(databaseId);  // Initial database refresh.
  await new Promise(waitDatabaseLoaded);       // Needed to initialize database view, otherwise
  idbDatabaseTreeElement.onselect(false);      // IDBDatabaseTreeElement.database would be undefined.
  var databaseView = idbDatabaseTreeElement._view;

  // Create first objectstore
  await ApplicationTestRunner.createObjectStoreAsync(databaseName, objectStoreName1, indexName, keyPath);
  await waitRefreshDatabase();
  TestRunner.addResult('Created first objectstore.');
  ApplicationTestRunner.dumpIndexedDBTree();

  // Create second objectstore
  await ApplicationTestRunner.createObjectStoreAsync(databaseName, objectStoreName2, indexName, keyPath);
  await waitRefreshDatabase();
  TestRunner.addResult('Created second objectstore.');
  ApplicationTestRunner.dumpIndexedDBTree();
  ApplicationTestRunner.dumpObjectStores();

  // Add entries
  await ApplicationTestRunner.addIDBValueAsync(databaseName, objectStoreName1, 'testKey', 'testValue');
  TestRunner.addResult('Added ' + objectStoreName1 + ' entry.');
  ApplicationTestRunner.dumpObjectStores();

  // Refresh database view
  await waitRefreshDatabase();
  await waitUpdateDataView();  // Wait for indexes and second object store to refresh.
  await waitUpdateDataView();
  await waitUpdateDataView();
  TestRunner.addResult('Refreshed database view.');
  ApplicationTestRunner.dumpObjectStores();

  // Add entries
  await ApplicationTestRunner.addIDBValueAsync(databaseName, objectStoreName2, 'testKey2', 'testValue2');
  TestRunner.addResult('Added ' + objectStoreName2 + ' entry.');
  ApplicationTestRunner.dumpObjectStores();

  // Right-click refresh database view
  await waitRefreshDatabaseRightClick();
  await waitUpdateDataView();  // Wait for indexes and second object store to refresh.
  await waitUpdateDataView();
  await waitUpdateDataView();
  TestRunner.addResult('Right-click refreshed database.');
  ApplicationTestRunner.dumpObjectStores();

  await ApplicationTestRunner.deleteDatabaseAsync(databaseName);
  TestRunner.completeTest();
})();
