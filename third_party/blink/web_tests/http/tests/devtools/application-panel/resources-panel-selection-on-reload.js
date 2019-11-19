// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests Application Panel response to a main frame navigation.\n`);
  await TestRunner.loadModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('resources');

  function createIndexedDB(callback) {
    var mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;
    var model = TestRunner.mainTarget.model(Resources.IndexedDBModel);
    ApplicationTestRunner.createDatabase(mainFrameId, 'Database1', () => {
      var event = model.addEventListener(Resources.IndexedDBModel.Events.DatabaseAdded, () => {
        Common.EventTarget.removeEventListeners([event]);
        callback();
      });
      model.refreshDatabaseNames();
    });
  }

  function dump(node, prefix) {
    for (var child of node.children()) {
      TestRunner.addResult(prefix + child.listItemElement.textContent);
      dump(child, prefix + '  ');
    }
  }

  function dumpCurrentState(label) {
    var view = UI.panels.resources;
    TestRunner.addResult(label);
    dump(view._sidebar._sidebarTree.rootElement(), '');
    var path = [];
    for (var selected = view._sidebar._sidebarTree.selectedTreeElement; selected; selected = selected.parent) {
      if (selected.itemURL)
        path.push(selected.itemURL);
    }
    TestRunner.addResult('Selection: ' + JSON.stringify(path));
    TestRunner.addResult('Visible view is a cookie view: ' + (view.visibleView instanceof Resources.CookieItemsView));
  }

  await new Promise(createIndexedDB);
  await ApplicationTestRunner.createWebSQLDatabase('database-for-test');
  UI.viewManager.showView('resources');
  UI.panels.resources._sidebar.cookieListTreeElement.firstChild().select(false, true);
  dumpCurrentState('Initial state:');
  await TestRunner.reloadPagePromise();
  dumpCurrentState('After navigation:');
  TestRunner.completeTest();
})();
