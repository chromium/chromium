// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Application from 'devtools/panels/application/application.js';

(async function() {
  TestRunner.addResult(
      `Tests that data is correctly loaded by IndexedDBModel from IndexedDB object store and index.\n`);
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  var indexedDBModel = ApplicationTestRunner.createIndexedDBModel();
  var mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;
  var storageKey = 'http://127.0.0.1:8000/';
  var databaseName = 'testDatabase';
  var objectStoreName1 = 'testObjectStore1';
  var objectStoreName2 = 'testObjectStore2';
  var indexName = 'testIndexName';
  var databaseId = new Application.IndexedDBModel.DatabaseId({storageKey}, databaseName);

  /**
   * @param {number} count
   * @param {boolean} specifyKey
   * @param {*} callback
   */
  function addIDBValues(count, objectStoreName, specifyKey, callback) {
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
      let insertedObject;
      if (specifyKey)
        insertedObject = {key: key, value: value};
      else
        insertedObject = {value: value};
      ApplicationTestRunner.addIDBValue(
          mainFrameId, databaseName, objectStoreName, insertedObject, '', addValues);
    }
  }

  function loadValuesAndDump(fromIndex, idbKeyRange, skipCount, pageSize, callback) {
    var idbKeyRangeString = idbKeyRange ? '{lower:"' + idbKeyRange.lower + '",lowerOpen:' + idbKeyRange.lowerOpen +
            ',upper:"' + idbKeyRange.upper + '",upperOpen:' + idbKeyRange.upperOpen + '}' :
                                          'null';
    TestRunner.addResult(
        'Dumping values, fromIndex = ' + fromIndex + ', skipCount = ' + skipCount + ', pageSize = ' + pageSize +
        ', idbKeyRange = ' + idbKeyRangeString);
    if (fromIndex)
      indexedDBModel.loadIndexData(
          databaseId, objectStoreName1, indexName, idbKeyRange, skipCount, pageSize, innerCallback);
    else
      indexedDBModel.loadObjectStoreData(databaseId, objectStoreName1, idbKeyRange, skipCount, pageSize, innerCallback);

    function innerCallback(entries, hasMore) {
      var index = 0;
      var entry;
      dumpEntries();

      function dumpEntries() {
        if (index === entries.length) {
          TestRunner.addResult('    hasMore = ' + hasMore);
          callback();
          return;
        }
        entry = entries[index++];
        entry.value.callFunctionJSON(dumpMe, undefined).then(dumped.bind(this));
      }

      function dumpMe() {
        return '{"key":"' + this.key + '","value":"' + this.value + '"}';
      }

      function dumped(value) {
        TestRunner.addResult(
            '    Key = ' + entry.key.description + ', primaryKey = ' + entry.primaryKey.description +
            ', value = ' + value);
        dumpEntries();
      }
    }
  }

  fillDatabase();

  function fillDatabase() {
    ApplicationTestRunner.createDatabase(mainFrameId, databaseName, step2);

    function step2() {
      ApplicationTestRunner.createObjectStore(
        mainFrameId, databaseName, objectStoreName1, 'key', true, () => {
          ApplicationTestRunner.createObjectStore(mainFrameId, databaseName, objectStoreName2, 'key', true, step3);
        });
    }

    function step3() {
      ApplicationTestRunner.createObjectStoreIndex(
          mainFrameId, databaseName, objectStoreName1, indexName, 'value', false, true, step4);
    }

    function step4() {
      addIDBValues(6, objectStoreName1, true, () => {
        addIDBValues(6, objectStoreName2, false, postFillingActions);
      });
    }

    async function postFillingActions() {
      await new Promise(resolve => {
        indexedDBModel.getMetadata(
          databaseId, {name: objectStoreName1, autoIncrement: true}).then(printMetadata);
        indexedDBModel.getMetadata(
          databaseId, {name: objectStoreName2, autoIncrement: true}).then(printMetadata);
        resolve();
      });
      TestRunner.addSniffer(Application.IndexedDBModel.IndexedDBModel.prototype, 'updateStorageKeyDatabaseNames', refreshDatabase, false);
      indexedDBModel.refreshDatabaseNames();

      function printMetadata(metadata) {
        if (!metadata) {
          TestRunner.addResult('backend returns an error response');
          return;
        }
        const entriesCount = metadata.entriesCount;
        const keyGenNumber = metadata.keyGeneratorValue;
        TestRunner.addResult('entries count: ' + String(entriesCount));
        TestRunner.addResult('key gen value: ' + String(keyGenNumber));
      }
    }
  }

  function refreshDatabase() {
    indexedDBModel.addEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, runObjectStoreTests);
    indexedDBModel.refreshDatabase(databaseId);
  }

  function runObjectStoreTests() {
    indexedDBModel.removeEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, runObjectStoreTests);
    loadValuesAndDump(false, null, 0, 2, step2);

    function step2() {
      loadValuesAndDump(false, null, 2, 2, step3);
    }

    function step3() {
      loadValuesAndDump(false, null, 4, 2, step4);
    }

    function step4() {
      loadValuesAndDump(false, null, 5, 2, step5);
    }

    function step5() {
      loadValuesAndDump(false, null, 6, 2, step6);
    }

    function step6() {
      loadValuesAndDump(false, null, 7, 2, step7);
    }

    function step7() {
      loadValuesAndDump(false, IDBKeyRange.bound('key_02', 'key_05', true, false), 0, 2, step8);
    }

    function step8() {
      loadValuesAndDump(false, IDBKeyRange.bound('key_02', 'key_05', true, false), 0, 3, step9);
    }

    function step9() {
      loadValuesAndDump(false, IDBKeyRange.bound('key_02', 'key_05', true, false), 0, 4, step10);
    }

    function step10() {
      loadValuesAndDump(false, IDBKeyRange.bound('key_02', 'key_05', true, false), 1, 2, step11);
    }

    function step11() {
      loadValuesAndDump(false, IDBKeyRange.bound('key_02', 'key_05', true, false), 1, 3, step12);
    }

    function step12() {
      loadValuesAndDump(false, IDBKeyRange.bound('key_02', 'key_05', true, false), 1, 4, step13);
    }

    function step13() {
      runIndexTests();
    }
  }

  function runIndexTests() {
    loadValuesAndDump(true, null, 0, 2, step2);

    function step2() {
      runClearTests();
    }
  }

  function runClearTests() {
    indexedDBModel.clearObjectStore(databaseId, objectStoreName1).then(step1);
    TestRunner.addResult('Cleared data from objectStore');

    function step1() {
      indexedDBModel.addEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step2);
      indexedDBModel.refreshDatabase(databaseId);
    }
    function step2() {
      indexedDBModel.removeEventListener(Application.IndexedDBModel.Events.DatabaseLoaded, step2);
      loadValuesAndDump(false, null, 0, 10, step3);
    }

    function step3() {
      deleteDatabase();
    }
  }

  function deleteDatabase() {
    ApplicationTestRunner.deleteObjectStoreIndex(mainFrameId, databaseName, objectStoreName1, indexName, step2);

    function step2() {
      ApplicationTestRunner.deleteObjectStore(mainFrameId, databaseName, objectStoreName1, () => {
        ApplicationTestRunner.deleteObjectStore(mainFrameId, databaseName, objectStoreName2, step3);
      });
    }

    function step3() {
      ApplicationTestRunner.deleteDatabase(mainFrameId, databaseName, step4);
    }

    async function step4() {
      await ConsoleTestRunner.dumpConsoleMessages();
      TestRunner.completeTest();
    }
  }
})();
