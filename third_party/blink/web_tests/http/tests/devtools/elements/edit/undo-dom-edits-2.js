// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that DOM modifications done in the Elements panel are undoable (Part 2).\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div style="display:none">
          <div id="testSetAttribute">
              <div foo="attribute value" id="node-to-set-attribute"></div>
          </div>

          <div id="testRemoveAttribute">
              <div foo="attribute value" id="node-to-remove-attribute"></div>
          </div>

          <div id="testAddAttribute">
              <div id="node-to-add-attribute"></div>
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

  function testSetAttribute(callback) {
    var node = ElementsTestRunner.expandedNodeWithId('node-to-set-attribute');
    node.setAttribute('foo', 'bar="edited attribute"', callback);
  }
  testSuite.push(ElementsTestRunner.generateUndoTest(testSetAttribute));


  function testRemoveAttribute(callback) {
    var node = ElementsTestRunner.expandedNodeWithId('node-to-remove-attribute');
    node.removeAttribute('foo').then(callback);
  }
  testSuite.push(ElementsTestRunner.generateUndoTest(testRemoveAttribute));


  function testAddAttribute(callback) {
    var node = ElementsTestRunner.expandedNodeWithId('node-to-add-attribute');
    node.setAttribute('', 'newattr="new-value"', callback);
  }
  testSuite.push(ElementsTestRunner.generateUndoTest(testAddAttribute));


  TestRunner.runTestSuite(testSuite);
})();
