// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as Application from 'devtools/panels/application/application.js';

(async function() {
  TestRunner.addResult(`Tests that database names are correctly loaded and saved in IndexedDBModel.\n`);
    //Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  var indexedDBModel = ApplicationTestRunner.createIndexedDBModel();
  var mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;

  function dumpDatabaseNames() {
    TestRunner.addResult('Dumping database names:');
    var storageKeys = TestRunner.storageKeyManager.storageKeys();
    var storageKey = storageKeys[0];
    var buckets =
        indexedDBModel.databaseNamesByStorageKeyAndBucket.get(storageKey) || [];
    for (const [_bucketName, dbIds] of buckets) {
      for (const dbId of dbIds) {
        TestRunner.addResult('    ' + dbId.name);
      }
    }
    TestRunner.addResult('');
 }

  step2();

  function step2() {
    dumpDatabaseNames();
    ApplicationTestRunner.createDatabase(mainFrameId, 'testDatabase1', step3);
  }

  function step3() {
    TestRunner.addSniffer(Application.IndexedDBModel.IndexedDBModel.prototype, 'updateStorageKeyDatabaseNames', step4, false);
    indexedDBModel.refreshDatabaseNames();
  }

  function step4() {
    dumpDatabaseNames();
    ApplicationTestRunner.createDatabase(mainFrameId, 'testDatabase2', step5);
  }

  function step5() {
    TestRunner.addSniffer(Application.IndexedDBModel.IndexedDBModel.prototype, 'updateStorageKeyDatabaseNames', step6, false);
    indexedDBModel.refreshDatabaseNames();
  }

  function step6() {
    dumpDatabaseNames();
    ApplicationTestRunner.deleteDatabase(mainFrameId, 'testDatabase2', step7);
  }

  function step7() {
    TestRunner.addSniffer(Application.IndexedDBModel.IndexedDBModel.prototype, 'updateStorageKeyDatabaseNames', step8, false);
    indexedDBModel.refreshDatabaseNames();
  }

  function step8() {
    dumpDatabaseNames();
    ApplicationTestRunner.deleteDatabase(mainFrameId, 'testDatabase1', step9);
  }

  function step9() {
    TestRunner.addSniffer(Application.IndexedDBModel.IndexedDBModel.prototype, 'updateStorageKeyDatabaseNames', step10, false);
    indexedDBModel.refreshDatabaseNames();
  }

  function step10() {
    dumpDatabaseNames();
    TestRunner.completeTest();
  }
})();
