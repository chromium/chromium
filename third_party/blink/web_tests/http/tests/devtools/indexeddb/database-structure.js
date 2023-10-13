// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Application from 'devtools/panels/application/application.js';

(async function() {
  TestRunner.addResult(`Tests that database names are correctly loaded and saved in IndexedDBModel.\n`);
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  var indexedDBModel = ApplicationTestRunner.createIndexedDBModel();
  var mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;
  var databaseName = 'testDatabase1';
  var storageKey = 'http://127.0.0.1:8000/';
  var databaseId = new Application.IndexedDBModel.DatabaseId({storageKey}, databaseName);

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
    ApplicationTestRunner.createDatabase(mainFrameId, databaseName, step3);
  }

  function step3() {
    TestRunner.addSniffer(Application.IndexedDBModel.IndexedDBModel.prototype, 'updateStorageKeyDatabaseNames', step4, false);
    indexedDBModel.refreshDatabaseNames();
  }

  function step4() {
    dumpDatabase();

    indexedDBModel.addEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step5);
    indexedDBModel.refreshDatabase(databaseId);
  }

  function step5() {
    indexedDBModel.removeEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step5);
    dumpDatabase();

    ApplicationTestRunner.createObjectStore(
        mainFrameId, databaseName, 'testObjectStore1', 'test.key.path', true, step6);
  }

  function step6() {
    indexedDBModel.addEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step7);
    indexedDBModel.refreshDatabase(databaseId);
  }

  function step7() {
    indexedDBModel.removeEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step7);
    dumpDatabase();

    ApplicationTestRunner.createObjectStore(mainFrameId, databaseName, 'testObjectStore2', null, false, step8);
  }

  function step8() {
    indexedDBModel.addEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step9);
    indexedDBModel.refreshDatabase(databaseId);
  }

  function step9() {
    indexedDBModel.removeEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step9);
    dumpDatabase();

    ApplicationTestRunner.createObjectStoreIndex(
        mainFrameId, databaseName, 'testObjectStore2', 'testIndexName1', '', false, true, step10);
  }

  function step10() {
    indexedDBModel.addEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step11);
    indexedDBModel.refreshDatabase(databaseId);
  }

  function step11() {
    indexedDBModel.removeEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step11);
    dumpDatabase();

    ApplicationTestRunner.createObjectStoreIndex(
        mainFrameId, databaseName, 'testObjectStore2', 'testIndexName2', ['key.path1', 'key.path2'], true, false,
        step12);
  }

  function step12() {
    indexedDBModel.addEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step13);
    indexedDBModel.refreshDatabase(databaseId);
  }

  function step13() {
    indexedDBModel.removeEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step13);
    dumpDatabase();

    ApplicationTestRunner.deleteObjectStoreIndex(
        mainFrameId, databaseName, 'testObjectStore2', 'testIndexName2', step14);
  }

  function step14() {
    indexedDBModel.addEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step15);
    indexedDBModel.refreshDatabase(databaseId);
  }

  function step15() {
    indexedDBModel.removeEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step15);
    dumpDatabase();

    ApplicationTestRunner.deleteObjectStoreIndex(
        mainFrameId, databaseName, 'testObjectStore2', 'testIndexName1', step16);
  }

  function step16() {
    indexedDBModel.addEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step17);
    indexedDBModel.refreshDatabase(databaseId);
  }

  function step17() {
    indexedDBModel.removeEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step17);
    dumpDatabase();

    ApplicationTestRunner.deleteObjectStore(mainFrameId, databaseName, 'testObjectStore2', step18);
  }

  function step18() {
    indexedDBModel.addEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step19);
    indexedDBModel.refreshDatabase(databaseId);
  }

  function step19() {
    indexedDBModel.removeEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step19);
    dumpDatabase();

    ApplicationTestRunner.deleteObjectStore(mainFrameId, databaseName, 'testObjectStore1', step20);
  }

  function step20() {
    indexedDBModel.addEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step21);
    indexedDBModel.refreshDatabase(databaseId);
  }

  function step21() {
    indexedDBModel.removeEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step21);
    dumpDatabase();
    ApplicationTestRunner.deleteDatabase(mainFrameId, databaseName, step22);
  }

  async function step22() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
