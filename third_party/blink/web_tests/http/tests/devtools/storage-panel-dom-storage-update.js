// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as Application from 'devtools/panels/application/application.js';

(async function() {
  TestRunner.addResult(
      `Test that storage panel is present and that it contains correct data whenever localStorage is updated.\n`);
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');
  await TestRunner.evaluateInPagePromise(`
      function addItem(key, value)
      {
          localStorage.setItem(key, value);
      }

      function removeItem(key)
      {
          localStorage.removeItem(key);
      }

      function updateItem(key, newValue)
      {
          localStorage.setItem(key, newValue);
      }

      function clear()
      {
          localStorage.clear();
      }
  `);

  var view = null;

  function dumpDataGrid(rootNode) {
    var nodes = rootNode.children;
    var rows = [];
    for (var i = 0; i < nodes.length; ++i) {
      var node = nodes[i];
      if (typeof node.data.key === 'string')
        rows.push(node.data.key + ' = ' + node.data.value);
    }
    rows.sort();
    TestRunner.addResult('Table rows: [' + rows.join(', ') + ']');
  }

  TestRunner.runTestSuite([
    function initialize(next) {
      TestRunner.evaluateInPage('clear();', next);
    },

    function updateLocalStorageView(next) {
      function viewUpdated(items) {
        TestRunner.addResult('Resource Panel with localStorage view updated.');
        next();
      }

      var storage = null;
      var storages = ApplicationTestRunner.domStorageModel().storages();
      for (var i = 0; i < storages.length; ++i) {
        if (storages[i].isLocalStorage) {
          storage = storages[i];
          break;
        }
      }

      TestRunner.assertTrue(!!storage, 'Local storage not found.');

      Application.ResourcesPanel.ResourcesPanel.instance().showDOMStorage(storage);
      view = Application.ResourcesPanel.ResourcesPanel.instance().domStorageView;
      TestRunner.addSniffer(view.grid, 'showItems', viewUpdated);
    },

    function addItemTest(next) {
      var indicesToAdd = [1, 2, 3, 4, 5, 6];

      function itemAdded() {
        dumpDataGrid(view.dataGridForTesting.rootNode());
        addItem();
      }

      function addItem() {
        var index = indicesToAdd.shift();
        if (!index) {
          next();
          return;
        }
        TestRunner.addResult('');
        TestRunner.addSniffer(ApplicationTestRunner.domStorageModel(), 'domStorageItemAdded', itemAdded);
        var command = 'addItem(\'key' + index + '\', \'value' + index + '\');';
        TestRunner.addResult(command);
        TestRunner.evaluateInPage(command);
      }

      addItem();
    },

    function removeItemTest(next) {
      var indicesToRemove = [1, 3, 5];

      function itemRemoved() {
        dumpDataGrid(view.dataGridForTesting.rootNode());
        removeItem();
      }

      function removeItem() {
        var index = indicesToRemove.shift();
        if (!index) {
          next();
          return;
        }
        TestRunner.addResult('');
        TestRunner.addSniffer(ApplicationTestRunner.domStorageModel(), 'domStorageItemRemoved', itemRemoved);
        var command = 'removeItem(\'key' + index + '\');';
        TestRunner.addResult(command);
        TestRunner.evaluateInPage(command);
      }

      removeItem();
    },

    function updateItemTest(next) {
      TestRunner.addResult('');
      TestRunner.addSniffer(ApplicationTestRunner.domStorageModel(), 'domStorageItemUpdated', itemUpdated);
      var command = 'updateItem(\'key2\', \'VALUE2\');';
      TestRunner.addResult(command);
      TestRunner.evaluateInPage(command);

      function itemUpdated() {
        dumpDataGrid(view.dataGridForTesting.rootNode());
        next();
      }
    },

    function clearTest(next) {
      function itemsCleared() {
        dumpDataGrid(view.dataGridForTesting.rootNode());
        next();
      }

      TestRunner.addResult('');
      TestRunner.addSniffer(ApplicationTestRunner.domStorageModel(), 'domStorageItemsCleared', itemsCleared);
      var command = 'clear()';
      TestRunner.addResult(command);
      TestRunner.evaluateInPage(command);
    }
  ]);
})();
