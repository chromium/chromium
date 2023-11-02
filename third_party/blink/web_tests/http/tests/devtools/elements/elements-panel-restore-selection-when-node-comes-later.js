// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Verify that last selected element is restored properly later, even if it failed to do so once.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.navigatePromise('./resources/elements-panel-restore-selection-when-node-comes-later.html');

  var node;

  TestRunner.runTestSuite([
    function selectNode(next) {
      ElementsTestRunner.nodeWithId('inspected', onNodeFound);

      function onNodeFound(n) {
        node = n;
        ElementsTestRunner.selectNode(node).then(onNodeSelected);
      }

      function onNodeSelected() {
        dumpSelectedNode();
        next();
      }
    },

    function firstReloadWithoutNodeInDOM(next) {
      TestRunner.addSniffer(Elements.ElementsPanel.prototype, 'lastSelectedNodeSelectedForTest', onNodeRestored);
      // Do a reload and pretend page's DOM doesn't have a node to restore.
      overridePushNodeForPath(node.path());
      TestRunner.reloadPage(function() {});

      function onNodeRestored() {
        dumpSelectedNode();
        next();
      }
    },

    function secondReloadWithNodeInDOM(next) {
      var pageReloaded = false;
      var nodeRestored = false;
      TestRunner.addSniffer(Elements.ElementsPanel.prototype, 'lastSelectedNodeSelectedForTest', onNodeRestored);
      TestRunner.reloadPage(onPageReloaded);

      function onPageReloaded() {
        pageReloaded = true;
        maybeNext();
      }

      function onNodeRestored() {
        nodeRestored = true;
        maybeNext();
      }

      function maybeNext() {
        if (!nodeRestored || !pageReloaded)
          return;
        dumpSelectedNode();
        next();
      }
    },

  ]);

  function dumpSelectedNode() {
    var selectedElement = ElementsTestRunner.firstElementsTreeOutline().selectedTreeElement;
    var nodeName = selectedElement ? selectedElement.node().nodeNameInCorrectCase() : 'null';
    TestRunner.addResult('Selected node: \'' + nodeName + '\'');
  }

  /**
   * @param {string} pathToIgnore
   */
  function overridePushNodeForPath(pathToIgnore) {
    var original = TestRunner.override(SDK.DOMModel.prototype, 'pushNodeByPathToFrontend', override);

    function override(nodePath) {
      if (nodePath === pathToIgnore)
        return Promise.resolve(null);
      return original(nodePath);
    }
  }
})();
