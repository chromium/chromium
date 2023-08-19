// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that database names are correctly loaded and saved in IndexedDBModel.\n`);
  await TestRunner.loadLegacyModule('console');
  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  var indexedDBModel = ApplicationTestRunner.createIndexedDBModel();
  var mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;
  var databaseName = 'testDatabase1';
  var storageKey = 'http://127.0.0.1:8000/';
  var databaseId = new Resources.IndexedDBModel.DatabaseId({storageKey}, databaseName);

  function dumpDatabase() {
    TestRunner.addResult('Dumping database:');
    const database = indexedDBModel.databasesInternal.get(databaseId);
    if (!database)
      return;
    TestRunner.addResult(database.databaseId.name);
    TestRunner.addResult('    version: ' + database.version);
    TestRunner.addResult('    objectStores:');
    const objectStoreNames = [...database.objectStores.keys()];
    objectStoreNames.sort();
    for (const objectStoreName of objectStoreNames) {
      const objectStore = database.objectStores.get(objectStoreName);
      TestRunner.addResult('    ' + objectStore.name);
      TestRunner.addResult('        keyPath: ' + JSON.stringify(objectStore.keyPath));
      TestRunner.addResult('        autoIncrement: ' + objectStore.autoIncrement);
      TestRunner.addResult('        indexes: ');
      const indexNames = [...objectStore.indexes.keys()];
      indexNames.sort();
      for (const indexName of indexNames) {
        const index = objectStore.indexes.get(indexName);
        TestRunner.addResult('        ' + index.name);
        TestRunner.addResult('            keyPath: ' + JSON.stringify(index.keyPath));
        TestRunner.addResult('            unique: ' + index.unique);
        TestRunner.addResult('            multiEntry: ' + index.multiEntry);
    }
  }
    TestRunner.addResult('');
}

  step2();

  function step2() {
    ApplicationTestRunner.createDatabaseWithVersion(mainFrameId, databaseName, 2147483647, step3);
  }

  function step3() {
    TestRunner.addSniffer(Resources.IndexedDBModel.prototype, 'updateStorageKeyDatabaseNames', step4, false);
    indexedDBModel.refreshDatabaseNames();
  }

  function step4() {
    dumpDatabase();

    indexedDBModel.addEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, step5);
    indexedDBModel.refreshDatabase(databaseId);
  }

  function step5() {
    indexedDBModel.removeEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, step5);
    dumpDatabase();

    ApplicationTestRunner.createObjectStore(
      mainFrameId, databaseName, 'testObjectStore1', 'test.key.path', true, step6);
  }

  function step6() {
    indexedDBModel.addEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, step7);
    indexedDBModel.refreshDatabase(databaseId);
  }

  function step7() {
    indexedDBModel.removeEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, step7);
    dumpDatabase();

    ApplicationTestRunner.createObjectStore(mainFrameId, databaseName, 'testObjectStore2', null, false, step8);
  }

  function step8() {
    indexedDBModel.addEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, step9);
    indexedDBModel.refreshDatabase(databaseId);
  }


  function step9() {
    indexedDBModel.removeEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, step9);
    dumpDatabase();
    ApplicationTestRunner.deleteDatabase(mainFrameId, databaseName, step10);
  }

  async function step10() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
