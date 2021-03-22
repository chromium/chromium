// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that user can mutate DOM by means of elements panel.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div>
          <div id="testSetAttribute">
              <div foo="attribute value" id="node-to-set-attribute"></div>
          </div>

          <div id="testSetScriptableAttribute">
              <div onclick="alert(1)" id="node-to-set-scriptable-attribute"></div>
          </div>

          <div id="testRemoveAttribute">
              <div foo="attribute value" id="node-to-remove-attribute"></div>
          </div>

          <div id="testAddAttribute">
              <div id="node-to-add-attribute"></div>
          </div>

          <div id="testAddAttributeUnquotedValue">
              <div id="node-to-add-attribute-unquoted-value"></div>
          </div>
      </div>
    `);

  // Save time on style updates.
  ElementsTestRunner.ignoreSidebarUpdates();

  TestRunner.runTestSuite([
    function testSetUp(next) {
      ElementsTestRunner.expandElementsTree(next);
    },

    function testSetAttribute(next) {
      ElementsTestRunner.domActionTestForNodeId('testSetAttribute', 'node-to-set-attribute', testBody, next);

      function testBody(node, done) {
        ElementsTestRunner.editNodePartAndRun(node, 'webkit-html-attribute', 'bar="edited attribute"', done, true);
      }
    },

    function testSetScriptableAttribute(next) {
      ElementsTestRunner.domActionTestForNodeId(
          'testSetScriptableAttribute', 'node-to-set-scriptable-attribute', testBody, next);

      function testBody(node, done) {
        ElementsTestRunner.editNodePartAndRun(node, 'webkit-html-attribute', 'onclick="alert(2)"', done, true);
      }
    },

    function testRemoveAttribute(next) {
      ElementsTestRunner.domActionTestForNodeId('testRemoveAttribute', 'node-to-remove-attribute', testBody, next);

      function testBody(node, done) {
        ElementsTestRunner.editNodePartAndRun(node, 'webkit-html-attribute', '', done, true);
      }
    },

    function testAddAttribute(next) {
      ElementsTestRunner.doAddAttribute('testAddAttribute', 'node-to-add-attribute', 'newattr="new-value"', next);
    },

    function testAddAttributeUnquotedValue(next) {
      ElementsTestRunner.doAddAttribute(
          'testAddAttributeUnquotedValue', 'node-to-add-attribute-unquoted-value', 'newattr=unquotedValue', next);
    },
  ]);
})();
