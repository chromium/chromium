// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that database names are correctly loaded and saved in IndexedDBModel.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
    //Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  var indexedDBModel = ApplicationTestRunner.createIndexedDBModel();
  var mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;

  function dumpDatabaseNames() {
    TestRunner.addResult('Dumping database names:');
    var storageKeys = TestRunner.storageKeyManager.storageKeys();
    var storageKey = storageKeys[0];
    var names = indexedDBModel.databaseNamesByStorageKey.get(storageKey);
    for (let name of names || [])
      TestRunner.addResult('    ' + name);
    TestRunner.addResult('');
  }

  TestRunner.addSniffer(Resources.IndexedDBModel.prototype, 'updateStorageKeyDatabaseNames', step2, false);

  function step2() {
    dumpDatabaseNames();
    ApplicationTestRunner.createDatabase(mainFrameId, 'testDatabase1', step3);
  }

  function step3() {
    TestRunner.addSniffer(Resources.IndexedDBModel.prototype, 'updateStorageKeyDatabaseNames', step4, false);
    indexedDBModel.refreshDatabaseNames();
  }

  function step4() {
    dumpDatabaseNames();
    ApplicationTestRunner.createDatabase(mainFrameId, 'testDatabase2', step5);
  }

  function step5() {
    TestRunner.addSniffer(Resources.IndexedDBModel.prototype, 'updateStorageKeyDatabaseNames', step6, false);
    indexedDBModel.refreshDatabaseNames();
  }

  function step6() {
    dumpDatabaseNames();
    ApplicationTestRunner.deleteDatabase(mainFrameId, 'testDatabase2', step7);
  }

  function step7() {
    TestRunner.addSniffer(Resources.IndexedDBModel.prototype, 'updateStorageKeyDatabaseNames', step8, false);
    indexedDBModel.refreshDatabaseNames();
  }

  function step8() {
    dumpDatabaseNames();
    ApplicationTestRunner.deleteDatabase(mainFrameId, 'testDatabase1', step9);
  }

  function step9() {
    TestRunner.addSniffer(Resources.IndexedDBModel.prototype, 'updateStorageKeyDatabaseNames', step10, false);
    indexedDBModel.refreshDatabaseNames();
  }

  function step10() {
    dumpDatabaseNames();
    TestRunner.completeTest();
  }
})();
