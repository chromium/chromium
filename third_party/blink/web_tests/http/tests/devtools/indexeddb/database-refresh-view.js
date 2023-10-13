// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Application from 'devtools/panels/application/application.js';

(async function() {
  TestRunner.addResult(`Tests refreshing the database information and data views.\n`);
  await TestRunner.navigatePromise('http://127.0.0.1:8000/devtools/indexeddb/resources/without-indexed-db.html');
  await ApplicationTestRunner.setupIndexedDBHelpers();

  // Note: every test that uses a storage API must manually clean-up state from
  // previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  var databaseName = 'testDatabase';
  var objectStoreName1 = 'testObjectStore1';
  var objectStoreName2 = 'testObjectStore2';
  var indexName = 'testIndex';
  var keyPath = 'testKey';

  var indexedDBModel = TestRunner.mainTarget.model(Application.IndexedDBModel.IndexedDBModel);
  indexedDBModel.throttler['#timeout'] = 100000;  // Disable live updating.
  var databaseId;

  function waitRefreshDatabase() {
    var view = Application.ResourcesPanel.ResourcesPanel.instance().sidebar.indexedDBListTreeElement.idbDatabaseTreeElements[0].view;

    view.getComponent().refreshDatabaseButtonClicked();
    return indexedDBModel.once(Application.IndexedDBModel.Events.DatabaseLoaded);
  }

  function waitRefreshDatabaseRightClick() {
    idbDatabaseTreeElement.refreshIndexedDB();
    return waitUpdateDataView();
  }

  function waitUpdateDataView() {
    return new Promise((resolve) => {
      TestRunner.addSniffer(Application.IndexedDBViews.IDBDataView.prototype, 'updatedDataForTests', resolve, false);
    });
  }

  function waitDatabaseLoaded(callback) {
    var event = indexedDBModel.addEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, () => {
      Common.EventTarget.removeEventListeners([event]);
      callback();
    });
  }

  function waitDatabaseAdded(callback) {
    var event = indexedDBModel.addEventListener(Application.IndexedDBModel.Events.DatabaseAdded, () => {
      Common.EventTarget.removeEventListeners([event]);
      callback();
    });
    Application.ResourcesPanel.ResourcesPanel.instance().sidebar.indexedDBListTreeElement.refreshIndexedDB();
  }

  // Initial tree
  ApplicationTestRunner.dumpIndexedDBTree();

  // Create database
  ApplicationTestRunner.createDatabaseAsync(databaseName);
  await new Promise(waitDatabaseAdded);
  var idbDatabaseTreeElement = Application.ResourcesPanel.ResourcesPanel.instance().sidebar.indexedDBListTreeElement.idbDatabaseTreeElements[0];
  databaseId = idbDatabaseTreeElement.databaseId;
  TestRunner.addResult('Created database.');
  ApplicationTestRunner.dumpIndexedDBTree();

  // Load indexedDb database view
  indexedDBModel.refreshDatabase(databaseId);  // Initial database refresh.
  await new Promise(waitDatabaseLoaded);       // Needed to initialize database view, otherwise
  idbDatabaseTreeElement.onselect(false);      // IDBDatabaseTreeElement.database would be undefined.
  var databaseView = idbDatabaseTreeElement.view;

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

  let onUpdate = () => {};

  TestRunner.addSniffer(
      Application.IndexedDBViews.IDBDataView.prototype, 'updatedDataForTests', function() {
        onUpdate(this);
      }, true);

  const NUM_EXPECTED_VIEWS = 4;  // Two object store views and two index views
  // Refresh database view
  let updates = new Promise(resolve => {
    const updated = new Set();
    onUpdate = (view) => {
      updated.add(view);
      if (updated.size === NUM_EXPECTED_VIEWS) resolve();
    };
  });
  await waitRefreshDatabase();
  await updates;
  TestRunner.addResult('Refreshed database view.');
  ApplicationTestRunner.dumpObjectStores();

  // Add entries
  await ApplicationTestRunner.addIDBValueAsync(databaseName, objectStoreName2, 'testKey2', 'testValue2');
  TestRunner.addResult('Added ' + objectStoreName2 + ' entry.');
  ApplicationTestRunner.dumpObjectStores();

  // Right-click refresh database view
  updates = new Promise(resolve => {
    const updated = new Set();
    onUpdate = (view) => {
      updated.add(view);
      if (updated.size === NUM_EXPECTED_VIEWS) resolve();
    };
  });
  await waitRefreshDatabaseRightClick();
  await updates;
  TestRunner.addResult('Right-click refreshed database.');
  ApplicationTestRunner.dumpObjectStores();

  await ApplicationTestRunner.deleteDatabaseAsync(databaseName);
  TestRunner.completeTest();
})();
