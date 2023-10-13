// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
import * as Application from 'devtools/panels/application/application.js';

(async function() {
  TestRunner.addResult(`Tests Application Panel response to a main frame navigation.\n`);
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  function createIndexedDB(callback) {
    var mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;
    var model = TestRunner.mainTarget.model(Application.IndexedDBModel.IndexedDBModel);
    ApplicationTestRunner.createDatabase(mainFrameId, 'Database1', () => {
      var event = model.addEventListener(Application.IndexedDBModel.Events.DatabaseAdded, () => {
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
    var view = Application.ResourcesPanel.ResourcesPanel.instance();
    TestRunner.addResult(label);
    dump(view.sidebar.sidebarTree.rootElement(), '');
    var path = [];
    for (var selected = view.sidebar.sidebarTree.selectedTreeElement; selected; selected = selected.parent) {
      if (selected.itemURL)
        path.push(selected.itemURL);
    }
    TestRunner.addResult('Selection: ' + JSON.stringify(path));
    TestRunner.addResult('Visible view is a cookie view: ' + (view.visibleView instanceof Application.CookieItemsView.CookieItemsView));
  }

  await new Promise(createIndexedDB);
  await UI.ViewManager.ViewManager.instance().showView('resources');
  Application.ResourcesPanel.ResourcesPanel.instance().sidebar.cookieListTreeElement.firstChild().select(false, true);
  dumpCurrentState('Initial state:');
  await TestRunner.reloadPagePromise();
  dumpCurrentState('After navigation:');
  TestRunner.completeTest();
})();
