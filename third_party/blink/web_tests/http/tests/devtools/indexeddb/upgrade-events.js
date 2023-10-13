// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as Application from 'devtools/panels/application/application.js';

(async function() {
  TestRunner.addResult(`Tests that deleted databases do not get recreated.\n`);
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  var indexedDBModel = ApplicationTestRunner.createIndexedDBModel();
  var mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;
  var storageKey = 'http://127.0.0.1:8000/';
  var databaseName = 'testDatabase - ' + self.location;
  var objectStoreName = 'testObjectStore';
  var databaseId = new Application.IndexedDBModel.DatabaseId({storageKey}, databaseName);

  function onConsoleError(callback) {
    var old = console.error;
    console.error = function(message) {
      console.error = old;
      TestRunner.addResult(message);
      callback();
    };
  }

  function addIDBValues(count, callback) {
    var i = 0;
    addValues();

    function addValues() {
      if (i == count) {
        callback();
        return;
      }
      ++i;
      var id = i < 10 ? '0' + i : i;
      var key = 'key_' + id;
      var value = 'value_' + id;
      ApplicationTestRunner.addIDBValue(
          mainFrameId, databaseName, objectStoreName, {key: key, value: value}, '', addValues);
    }
  }

  function loadValues(skipCount, pageSize, callback) {
    indexedDBModel.loadObjectStoreData(databaseId, objectStoreName, null, skipCount, pageSize, innerCallback);

    function innerCallback(entries, hasMore) {
      callback();
    }
  }

  fillDatabase();

  function fillDatabase() {
    TestRunner.addResult('Preparing database');
    ApplicationTestRunner.deleteDatabase(mainFrameId, databaseName, step2);

    function step2() {
      ApplicationTestRunner.createDatabase(mainFrameId, databaseName, step3);
    }

    function step3() {
      ApplicationTestRunner.createObjectStore(mainFrameId, databaseName, objectStoreName, 'key', true, step4);
    }

    function step4() {
      addIDBValues(6, step5);
    }

    function step5() {
      TestRunner.addResult('Verifying that database was created');
      checkDatabaseDoesExist(loadValuesAndDeleteDatabase);
    }
  }

  function loadValuesAndDeleteDatabase() {
    TestRunner.addResult('Loading values');
    loadValues(0, 2, step2);

    function step2() {
      TestRunner.addResult('Deleting database');
      ApplicationTestRunner.deleteDatabase(mainFrameId, databaseName, step3);
    }

    function step3() {
      // ... but access the database again. This will implicitly attempt to recreate it,
      // which should be aborted during the "upgradeneeded" event.
      TestRunner.addResult('Loading values again (which should fail)');
      loadValues(0, 2, function() {
        TestRunner.addResult('Unexpected callback');
      });
      onConsoleError(step4);
    }

    function step4() {
      TestRunner.addResult('Verifying that database was not recreated');
      checkDatabaseDoesNotExist(step5);
    }

    function step5() {
      TestRunner.completeTest();
    }
  }

  function checkDatabaseDoesExist(callback) {
    TestRunner.addSniffer(Application.IndexedDBModel.IndexedDBModel.prototype, 'updateStorageKeyDatabaseNames', step2, false);
    indexedDBModel.refreshDatabaseNames();

    function step2() {
      var names = indexedDBModel.databaseNamesByStorageKeyAndBucket.get(storageKey).get('');
      TestRunner.assertEquals(true, !![...names].find(dbId => dbId.name === databaseName), 'Database should exist');
      callback();
    }
  }

  function checkDatabaseDoesNotExist(callback) {
    TestRunner.addSniffer(Application.IndexedDBModel.IndexedDBModel.prototype, 'updateStorageKeyDatabaseNames', step2, false);
    indexedDBModel.refreshDatabaseNames();

    function step2() {
      var names = indexedDBModel.databaseNamesByStorageKeyAndBucket.get(storageKey).get('');
      TestRunner.assertEquals(false, !![...names].find(dbId => dbId.name === databaseName), 'Database should not exist');
      callback();
    }
  }
})();
