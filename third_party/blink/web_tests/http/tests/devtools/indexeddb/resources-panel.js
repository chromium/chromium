// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests IndexedDB tree element on resources panel.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');

  var mainFrameId;
  var indexedDBModel;
  var withoutIndexedDBURL = 'http://localhost:8000/devtools/indexeddb/resources/without-indexed-db.html';
  var originalURL = 'http://127.0.0.1:8000/devtools/indexeddb/resources/with-indexed-db.html';
  var databaseName = 'testDatabase';
  var objectStoreName = 'testObjectStore';
  var indexName = 'testIndexName';

  function createDatabase(callback) {
    ApplicationTestRunner.createDatabase(mainFrameId, databaseName, step2);

    function step2() {
      ApplicationTestRunner.createObjectStore(mainFrameId, databaseName, objectStoreName, '', false, step3);
    }

    function step3() {
      ApplicationTestRunner.createObjectStoreIndex(
          mainFrameId, databaseName, objectStoreName, indexName, '', false, false, callback);
    }
  }

  function deleteDatabase(callback) {
    ApplicationTestRunner.deleteObjectStoreIndex(mainFrameId, databaseName, objectStoreName, indexName, step2);

    function step2() {
      ApplicationTestRunner.deleteObjectStore(mainFrameId, databaseName, objectStoreName, step3);
    }

    function step3() {
      ApplicationTestRunner.deleteDatabase(mainFrameId, databaseName, callback);
    }
  }

  await TestRunner.navigatePromise(originalURL);
  await ApplicationTestRunner.setupIndexedDBHelpers();
  mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;

  // Start with non-resources panel.
  await TestRunner.showPanel('console');

  // Switch to resources panel.
  await TestRunner.showPanel('resources');

  TestRunner.addResult('Expanded IndexedDB tree element.');
  UI.panels.resources.sidebar.indexedDBListTreeElement.expand();
  ApplicationTestRunner.dumpIndexedDBTree();
  TestRunner.addResult('Creating database.');
  createDatabase(databaseCreated);

  function databaseCreated() {
    TestRunner.addResult('Created database.');
    indexedDBModel = ApplicationTestRunner.indexedDBModel();
    indexedDBModel.addEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, databaseLoaded);
    UI.panels.resources.sidebar.indexedDBListTreeElement.refreshIndexedDB();
    TestRunner.addResult('Refreshing.');
  }

  async function databaseLoaded() {
    TestRunner.addResult('Refreshed.');
    indexedDBModel.removeEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, databaseLoaded);
    ApplicationTestRunner.dumpIndexedDBTree();
    TestRunner.addResult('Navigating to another security origin.');
    const dbRemoval = indexedDBModel.once(Resources.IndexedDBModel.Events.DatabaseRemoved);
    const navigationPromise = new Promise(resolve =>
      TestRunner.deprecatedRunAfterPendingDispatches(() =>
        TestRunner.navigatePromise(withoutIndexedDBURL).then(resolve))
    );
    await Promise.all([dbRemoval, navigationPromise]);
    navigatedAway();
  }

  function navigatedAway() {
    TestRunner.addResult('Navigated to another security origin.');
    indexedDBModel.removeEventListener(Resources.IndexedDBModel.Events.DatabaseRemoved);
    ApplicationTestRunner.dumpIndexedDBTree();
    TestRunner.addResult('Navigating back.');
    TestRunner.deprecatedRunAfterPendingDispatches(() => TestRunner.navigate(originalURL, navigatedBack));
  }

  function navigatedBack() {
    TestRunner.addResult('Navigated back.');
    indexedDBModel.addEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, databaseLoaded2);
    UI.panels.resources.sidebar.indexedDBListTreeElement.refreshIndexedDB();
    TestRunner.addResult('Refreshing.');
  }

  async function databaseLoaded2() {
    TestRunner.addResult('Refreshed.');
    indexedDBModel.removeEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, databaseLoaded2);
    ApplicationTestRunner.dumpIndexedDBTree();
    await ApplicationTestRunner.setupIndexedDBHelpers();
    mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;
    TestRunner.addResult('Deleting database.');
    deleteDatabase(databaseDeleted);
  }

  function databaseDeleted() {
    TestRunner.addResult('Deleted database.');
    TestRunner.addResult('Refreshing.');
    UI.panels.resources.sidebar.indexedDBListTreeElement.refreshIndexedDB();
    TestRunner.addSniffer(
        Resources.IndexedDBModel.prototype, 'updateStorageKeyDatabaseNames', databaseNamesLoadedAfterDeleting, false);
  }

  function databaseNamesLoadedAfterDeleting() {
    TestRunner.addResult('Refreshed.');
    ApplicationTestRunner.dumpIndexedDBTree();
    UI.panels.resources.sidebar.indexedDBListTreeElement.collapse();
    TestRunner.completeTest();
  }
})();
