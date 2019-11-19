// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Validate IndexeddbModel clearForOrigin\n`);
  await TestRunner.loadModule('application_test_runner');
  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('resources');

  const model = TestRunner.mainTarget.model(Resources.IndexedDBModel);
  const view = UI.panels.resources;

  function createIndexedDBInMainFrame(callback) {
    var mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;
    ApplicationTestRunner.createDatabase(mainFrameId, 'Database-main-frame', () => {
      var event = model.addEventListener(Resources.IndexedDBModel.Events.DatabaseAdded, () => {
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
      TestRunner.addResult(`  Security Origin:${databases[j].securityOrigin}, Database Name:${databases[j].name}`);
    TestRunner.addResult('**done**\n');
  }

  TestRunner.addResult('Create IndexedDB in main frame');
  await new Promise(createIndexedDBInMainFrame);
  await TestRunner.addSnifferPromise(UI.panels.resources._sidebar.indexedDBListTreeElement, '_indexedDBLoadedForTest');
  dumpDatabases();

  TestRunner.addResult('Removing bogus security origin...');
  model.clearForOrigin('http://bogus-security-origin.com');
  dumpDatabases();

  TestRunner.addResult('Removing http://127.0.0.1:8000 security origin...');
  model.clearForOrigin('http://127.0.0.1:8000');
  dumpDatabases();

  TestRunner.completeTest();
})();
