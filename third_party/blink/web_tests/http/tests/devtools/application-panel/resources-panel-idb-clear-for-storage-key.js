// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Application from 'devtools/panels/application/application.js';

(async function() {
  TestRunner.addResult(`Validate IndexeddbModel clearForStorageKey\n`);
  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  const model = TestRunner.mainTarget.model(Application.IndexedDBModel.IndexedDBModel);
  const view = Application.ResourcesPanel.ResourcesPanel.instance();

  function createIndexedDBInMainFrame(callback) {
    var mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;
    ApplicationTestRunner.createDatabase(mainFrameId, 'Database-main-frame', () => {
      var event = model.addEventListener(Application.IndexedDBModel.Events.DatabaseAdded, () => {
        Common.EventTarget.removeEventListeners([event]);
        callback();
      });
      model.refreshDatabaseNames();
    });
  }

  function dumpDatabases() {
    const databases = model.databases();
    TestRunner.addResult('Database Length: ' + databases.length);
    TestRunner.addResult('Database Entries:');
    for (let j = 0; j < databases.length; ++j)
      TestRunner.addResult(`  Storage key:${databases[j].storageBucket.storageKey}, Database Name:${databases[j].name}`);
    TestRunner.addResult('**done**\n');
  }

  TestRunner.addResult('Create IndexedDB in main frame');
  await new Promise(createIndexedDBInMainFrame);
  await TestRunner.addSnifferPromise(Application.ResourcesPanel.ResourcesPanel.instance().sidebar.indexedDBListTreeElement, 'indexedDBLoadedForTest');
  dumpDatabases();

  TestRunner.addResult('Removing bogus security origin...');
  model.clearForStorageKey('http://bogus-security-origin.com/');
  dumpDatabases();

  TestRunner.addResult('Removing http://127.0.0.1:8000 security origin...');
  model.clearForStorageKey('http://127.0.0.1:8000/');
  dumpDatabases();

  TestRunner.completeTest();
})();
