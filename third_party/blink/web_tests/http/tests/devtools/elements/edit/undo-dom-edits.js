// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that DOM modifications done in the Elements panel are undoable.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div style="display:none">
          <div id="testRemove">
              <div id="node-to-remove"></div>
          </div>

          <div id="testSetNodeName">
              <div id="node-to-set-name"></div>
          </div>

          <div id="testSetNodeValue">
              <div id="node-to-set-value">Text</div>
          </div>

          <div id="testEditAsHTML">
              <div id="node-to-edit-as-html"><span id="span">Text</span></div>
          </div>
      </div>
    `);

  var testSuite = [];

  function testSetUp(next) {
    TestRunner.addResult('Setting up');
    ElementsTestRunner.expandElementsTree(callback);

    function callback() {
      ElementsTestRunner.expandElementsTree(next);
    }
  }
  testSuite.push(testSetUp);


  function testRemove(callback) {
    var node = ElementsTestRunner.expandedNodeWithId('node-to-remove');
    node.removeNode(callback);
  }
  testSuite.push(ElementsTestRunner.generateUndoTest(testRemove));


  function testSetNodeName(callback) {
    var node = ElementsTestRunner.expandedNodeWithId('node-to-set-name');
    node.setNodeName('span', callback);
  }
  testSuite.push(ElementsTestRunner.generateUndoTest(testSetNodeName));


  function testSetNodeValue(callback) {
    var node = ElementsTestRunner.expandedNodeWithId('node-to-set-value');
    node.firstChild.setNodeValue('New Text', callback);
  }
  testSuite.push(ElementsTestRunner.generateUndoTest(testSetNodeValue));

  function testEditAsHTML(callback) {
    var node = ElementsTestRunner.expandedNodeWithId('node-to-edit-as-html');
    node.setOuterHTML(
        '<div id="node-to-edit-as-html"><div id="span2">Text2</div></div><span>Second node</span>', callback);
  }
  testSuite.push(ElementsTestRunner.generateUndoTest(testEditAsHTML));

  TestRunner.runTestSuite(testSuite);
})();
