// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that user can mutate DOM by means of elements panel.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div>
          <div id="testRemove">
              <div id="node-to-remove"></div>
          </div>

          <div id="testSetNodeName">
              <div id="node-to-set-name"></div>
          </div>

          <div id="testSetNodeNameInput">
              <div id="node-to-set-name-input"></div>
          </div>

          <div id="testSetNodeValue">
              <div id="node-to-set-value">
                Text
              </div>
          </div>
      </div>
    `);

  // Save time on style updates.
  ElementsTestRunner.ignoreSidebarUpdates();

  TestRunner.runTestSuite([
    function testSetUp(next) {
      ElementsTestRunner.expandElementsTree(next);
    },

    function testRemove(next) {
      ElementsTestRunner.domActionTestForNodeId('testRemove', 'node-to-remove', testBody, next);

      function testBody(node, done) {
        var treeElement = ElementsTestRunner.firstElementsTreeOutline().findTreeElement(node);
        treeElement.remove();
        TestRunner.deprecatedRunAfterPendingDispatches(done);
      }
    },

    function testSetNodeName(next) {
      ElementsTestRunner.domActionTestForNodeId('testSetNodeName', 'node-to-set-name', testBody, next);

      function testBody(node, done) {
        ElementsTestRunner.editNodePartAndRun(node, 'webkit-html-tag-name', 'span', done);
      }
    },

    function testSetNodeNameInput(next) {
      ElementsTestRunner.domActionTestForNodeId('testSetNodeNameInput', 'node-to-set-name-input', testBody, next);

      function testBody(node, done) {
        ElementsTestRunner.editNodePartAndRun(node, 'webkit-html-tag-name', 'input', done);
      }
    },

    function testSetNodeValue(next) {
      ElementsTestRunner.domActionTestForNodeId('testSetNodeValue', 'node-to-set-value', testBody, next);

      function testBody(node, done) {
        ElementsTestRunner.editNodePartAndRun(node, 'webkit-html-text-node', '  \n  Edited Text  \n  ', done);
      }
    },
  ]);
})();
