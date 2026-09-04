// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ApplicationTestRunner} from 'application_test_runner';
import * as Application from 'devtools/panels/application/application.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(
      `Test that storage panel is present and that it contains correct data for local and session DOM storages.\n`);
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');
  await TestRunner.evaluateInPagePromise(`
      function populateDOMStorage()
      {
          localStorage.clear();
          sessionStorage.clear();
          localStorage["resource-history"] = "{}"; // This mitigates file:// localstorage conflict.
          // Fill local and session storage with some random strings.
          for (var i = 0; i < 10; i++) {
              localStorage["a" + i] = "=value" + i + ", ";
              sessionStorage["b" + i] = "=value" + (i+10) + ", ";
          }
      }
  `);

  await TestRunner.evaluateInPagePromise('populateDOMStorage()');
  TestRunner.addResult('Populated local and session storage');

  function name(storage) {
    return storage.isLocalStorage ? 'Local storage' : 'Session storage';
  }

  function dumpDataGridContent(dataGrid) {
    var nodes = dataGrid.querySelectorAll('tr');
    var rows = [];
    for (var i = 0; i < nodes.length; ++i) {
      var cells = nodes[i].querySelectorAll('td');
      if (cells.length) {
        rows.push(cells[0].textContent + cells[1].textContent);
      }
    }
    rows.sort();
    TestRunner.addResult('KeyValue pairs: ' + rows.join(''));
  }

  var storages = ApplicationTestRunner.domStorageModel().storages();
  if (!storages || !storages.length) {
    TestRunner.addResult('FAIL: no DOM storages found.');
    TestRunner.completeTest();
    return;
  }

  for (const storage of storages) {
    const showItemsPromise = TestRunner.addSnifferPromise(
        Application.DOMStorageItemsView.DOMStorageItemsView.prototype,
        'showItems');
    Application.ResourcesPanel.ResourcesPanel.instance().showDOMStorage(storage);
    TestRunner.addResult('Did show: ' + name(storage));
    await showItemsPromise;
    await UI.Widget.Widget.allUpdatesComplete;
    TestRunner.addResult(name(storage) + ' content: ');
    var view =
        Application.ResourcesPanel.ResourcesPanel.instance().domStorageView;
    dumpDataGridContent(
        view.contentElement.querySelector('devtools-data-grid'));
  }

  TestRunner.addResult('DONE');
  TestRunner.completeTest();
})();
