// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that elements panel updates dom tree structure upon node removal.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container"><div id="child1">Text</div><div id="child2"></div><div id="child3"></div><div id="child4"></div></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function removeNode(id)
      {
          var child = document.getElementById(id);
          child.parentNode.removeChild(child);
      }

      function removeTextNode(id)
      {
          document.getElementById(id).textContent = "";
      }
  `);

  var containerNode;

  TestRunner.runTestSuite([
    function testDumpInitial(next) {
      function callback(node) {
        containerNode = ElementsTestRunner.expandedNodeWithId('container');

        TestRunner.addResult('========= Original ========');
        ElementsTestRunner.dumpElementsTree(containerNode);
        next();
      }
      ElementsTestRunner.expandElementsTree(callback);
    },

    function testRemoveTextNode(next) {
      function callback() {
        TestRunner.addResult('===== Removed Text node =====');
        ElementsTestRunner.dumpElementsTree(containerNode);
        next();
      }
      TestRunner.evaluateInPage('removeTextNode(\'child1\')', callback);
    },

    function testRemoveFirst(next) {
      function callback() {
        TestRunner.addResult('===== Removed first =====');
        ElementsTestRunner.dumpElementsTree(containerNode);
        next();
      }
      TestRunner.evaluateInPage('removeNode(\'child1\')', callback);
    },

    function testRemoveMiddle(next) {
      function callback() {
        TestRunner.addResult('===== Removed middle =====');
        ElementsTestRunner.dumpElementsTree(containerNode);
        next();
      }
      TestRunner.evaluateInPage('removeNode(\'child3\')', callback);
    },

    function testRemoveLast(next) {
      function callback() {
        TestRunner.addResult('===== Removed last =====');
        ElementsTestRunner.dumpElementsTree(containerNode);
        next();
      }
      TestRunner.evaluateInPage('removeNode(\'child4\')', callback);
    },

    function testRemoveTheOnly(next) {
      function callback() {
        TestRunner.addResult('===== Removed the only =====');
        ElementsTestRunner.dumpElementsTree(containerNode);
        next();
      }
      TestRunner.evaluateInPage('removeNode(\'child2\')', callback);
    }
  ]);
})();
