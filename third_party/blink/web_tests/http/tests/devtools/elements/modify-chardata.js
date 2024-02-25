// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that elements panel updates dom tree structure upon changes to characters.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="node">Foo</div>
      <div id="rangenode">Lorem ipsum dolor sit amet</div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function modifyChars()
      {
          var node = document.getElementById("node");
          node.firstChild.textContent = "Bar";
      }

      function modifyViaRange()
      {
          var range = document.createRange();
          var referenceNode = document.getElementById("rangenode").firstChild;
          range.selectNode(referenceNode);
          range.setStart(referenceNode, 9);
          range.setEnd(referenceNode, 9 + 5);
          range.deleteContents();
          var span = range.startContainer.ownerDocument.createElement("span");
          span.innerHTML = "test";
          range.insertNode(span);
      }
  `);

  var targetNode;

  TestRunner.runTestSuite([
    function testDumpInitial(next) {
      function callback(node) {
        targetNode = node;
        TestRunner.addResult('========= Original for normal mutation ========');
        ElementsTestRunner.dumpElementsTree(targetNode);
        next();
      }
      ElementsTestRunner.selectNodeWithId('node', callback);
    },

    function testSetAttribute(next) {
      function callback() {
        TestRunner.addResult('===== Mutated text node =====');
        ElementsTestRunner.dumpElementsTree(targetNode);
        next();
      }
      TestRunner.evaluateInPage('modifyChars()', callback);
    },

    function testModifyViaRange(next) {
      function callback() {
        TestRunner.addResult('===== Contents modified via Range =====');
        ElementsTestRunner.dumpElementsTree(targetNode);
        next();
      }
      function expandCallback() {
        ElementsTestRunner.expandElementsTree(callback);
      }
      function selectCallback(node) {
        targetNode = node;
        TestRunner.addResult('========= Original for Range mutation ========');
        ElementsTestRunner.dumpElementsTree(targetNode);
        TestRunner.evaluateInPage('modifyViaRange()', expandCallback);
      }
      ElementsTestRunner.selectNodeWithId('rangenode', selectCallback);
    }
  ]);
})();
