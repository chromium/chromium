// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests Application Panel's handling of storages in iframes.\n`);
  await TestRunner.loadModule('application_test_runner');
  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('resources');

  function createIndexedDBInMainFrame(callback) {
    var mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;
    var model = TestRunner.mainTarget.model(Resources.IndexedDBModel);
    ApplicationTestRunner.createDatabase(mainFrameId, 'Database-main-frame', () => {
      var event = model.addEventListener(Resources.IndexedDBModel.Events.DatabaseAdded, () => {
        Common.EventTarget.removeEventListeners([event]);
        callback();
      });
      model.refreshDatabaseNames();
    });
  }

  function dumpTree(node, level) {
    for (var child of node.children()) {
      TestRunner.addResult(' '.repeat(level) + child.listItemElement.textContent);
      dumpTree(child, level + 1);
    }
  }

  // create IndexedDB in iframe
  await TestRunner.addIframe('http://localhost:8000/devtools/application-panel/resources/indexeddb-in-iframe.html', {'id': 'indexeddb_page'});
  await TestRunner.addSnifferPromise(UI.panels.resources._sidebar.indexedDBListTreeElement, '_indexedDBLoadedForTest');

  // create IndexedDB in main frame
  await new Promise(createIndexedDBInMainFrame);
  await TestRunner.addSnifferPromise(UI.panels.resources._sidebar.indexedDBListTreeElement, '_indexedDBLoadedForTest');

  const view = UI.panels.resources;

  TestRunner.addResult('Initial tree...\n');

  dumpTree(view._sidebar._sidebarTree.rootElement(), 0);

  TestRunner.addResult('\nRemove iframe from page...\n');
  await TestRunner.evaluateInPageAsync(`
    (function(){
      let iframe = document.getElementById('indexeddb_page');
      iframe.parentNode.removeChild(iframe);
    })();
  `);

  dumpTree(view._sidebar._sidebarTree.rootElement(), 0);

  TestRunner.addResult('\nAdd iframe to page again...\n');
  await TestRunner.addIframe('http://localhost:8000/devtools/application-panel/resources/indexeddb-in-iframe.html', {'id': 'indexeddb_page'});
  await TestRunner.addSnifferPromise(UI.panels.resources._sidebar.indexedDBListTreeElement, '_indexedDBLoadedForTest');

  dumpTree(view._sidebar._sidebarTree.rootElement(), 0);

  TestRunner.completeTest();
})();
