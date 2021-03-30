// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Test that storage panel is present and that it contains correct data whenever localStorage is updated.\n`);
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('application_test_runner');
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
      if (typeof node._data.key === 'string')
        rows.push(node._data.key + ' = ' + node._data.value);
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

      UI.panels.resources.showDOMStorage(storage);
      view = UI.panels.resources._domStorageView;
      TestRunner.addSniffer(view, '_showDOMStorageItems', viewUpdated);
    },

    function addItemTest(next) {
      var indicesToAdd = [1, 2, 3, 4, 5, 6];

      function itemAdded() {
        dumpDataGrid(view._dataGrid.rootNode());
        addItem();
      }

      function addItem() {
        var index = indicesToAdd.shift();
        if (!index) {
          next();
          return;
        }
        TestRunner.addResult('');
        TestRunner.addSniffer(ApplicationTestRunner.domStorageModel(), '_domStorageItemAdded', itemAdded);
        var command = 'addItem(\'key' + index + '\', \'value' + index + '\');';
        TestRunner.addResult(command);
        TestRunner.evaluateInPage(command);
      }

      addItem();
    },

    function removeItemTest(next) {
      var indicesToRemove = [1, 3, 5];

      function itemRemoved() {
        dumpDataGrid(view._dataGrid.rootNode());
        removeItem();
      }

      function removeItem() {
        var index = indicesToRemove.shift();
        if (!index) {
          next();
          return;
        }
        TestRunner.addResult('');
        TestRunner.addSniffer(ApplicationTestRunner.domStorageModel(), '_domStorageItemRemoved', itemRemoved);
        var command = 'removeItem(\'key' + index + '\');';
        TestRunner.addResult(command);
        TestRunner.evaluateInPage(command);
      }

      removeItem();
    },

    function updateItemTest(next) {
      TestRunner.addResult('');
      TestRunner.addSniffer(ApplicationTestRunner.domStorageModel(), '_domStorageItemUpdated', itemUpdated);
      var command = 'updateItem(\'key2\', \'VALUE2\');';
      TestRunner.addResult(command);
      TestRunner.evaluateInPage(command);

      function itemUpdated() {
        dumpDataGrid(view._dataGrid.rootNode());
        next();
      }
    },

    function clearTest(next) {
      function itemsCleared() {
        dumpDataGrid(view._dataGrid.rootNode());
        next();
      }

      TestRunner.addResult('');
      TestRunner.addSniffer(ApplicationTestRunner.domStorageModel(), '_domStorageItemsCleared', itemsCleared);
      var command = 'clear()';
      TestRunner.addResult(command);
      TestRunner.evaluateInPage(command);
    }
  ]);
})();
