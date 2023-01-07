// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests Application Panel's handling of storages in iframes.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
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
  // We are enforcing OOPIFs here, so that we are sure that the bots on all platforms create an OOPIF.
  // This would not be guaranteed otherwise and could potentially result in a different display name
  // for the iframe in the frame tree.
  await TestRunner.addIframe('http://devtools.oopif.test:8000/devtools/application-panel/resources/indexeddb-in-iframe.html', {'id': 'indexeddb_page'});

  await TestRunner.addSnifferPromise(UI.panels.resources.sidebar.indexedDBListTreeElement, 'indexedDBLoadedForTest');

  // create IndexedDB in main frame
  await new Promise(createIndexedDBInMainFrame);
  await TestRunner.addSnifferPromise(UI.panels.resources.sidebar.indexedDBListTreeElement, 'indexedDBLoadedForTest');

  const view = UI.panels.resources;

  TestRunner.addResult('Initial tree...\n');

  dumpTree(view.sidebar.sidebarTree.rootElement(), 0);

  TestRunner.addResult('\nRemove iframe from page...\n');
  await TestRunner.evaluateInPageAsync(`
    (function(){
      let iframe = document.getElementById('indexeddb_page');
      iframe.parentNode.removeChild(iframe);
    })();
  `);

  dumpTree(view.sidebar.sidebarTree.rootElement(), 0);

  TestRunner.addResult('\nAdd iframe to page again...\n');
  await TestRunner.addIframe('http://devtools.oopif.test:8000/devtools/application-panel/resources/indexeddb-in-iframe.html', {'id': 'indexeddb_page'});
  await TestRunner.addSnifferPromise(UI.panels.resources.sidebar.indexedDBListTreeElement, 'indexedDBLoadedForTest');

  dumpTree(view.sidebar.sidebarTree.rootElement(), 0);

  TestRunner.completeTest();
})();
