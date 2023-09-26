// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that elements panel updates dom tree structure upon node insertion.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container"><div id="child1"></div><div id="child2"></div><div id="child3"></div></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function insertBeforeFirst()
      {
          var container = document.getElementById("container");
          var child = document.createElement("div");
          child.setAttribute("id", "child-before");
          container.insertBefore(child, container.firstChild);
      }

      function insertNode()
      {
          var container = document.getElementById("container");
          var child2 = document.getElementById("child2");
          var child = document.createElement("div");
          child.setAttribute("id", "child-middle");
          container.insertBefore(child, child2);
      }

      function appendChild()
      {
          var container = document.getElementById("container");
          var child = document.createElement("div");
          child.setAttribute("id", "child-after");
          container.appendChild(child);
      }

      function appendChildWithText()
      {
          var container = document.getElementById("container");
          var child = document.createElement("div");
          child.setAttribute("id", "child-with-text");
          child.setAttribute("style", "display: none;");
          child.innerText = "Text";
          container.appendChild(child);
      }

      function insertFirstTextNode()
      {
          var child3 = document.getElementById("child3");
          child3.innerText = "First text";
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

    function testInsertBefore(next) {
      function callback() {
        TestRunner.addResult('===== Inserted before =====');
        ElementsTestRunner.dumpElementsTree(containerNode);
        next();
      }
      TestRunner.evaluateInPage('insertBeforeFirst()', callback);
    },

    function testInsertMiddle(next) {
      function callback() {
        TestRunner.addResult('===== Inserted middle =====');
        ElementsTestRunner.dumpElementsTree(containerNode);
        next();
      }
      TestRunner.evaluateInPage('insertNode()', callback);
    },

    function testAppend(next) {
      function callback() {
        TestRunner.addResult('======== Appended =========');
        ElementsTestRunner.dumpElementsTree(containerNode);
        next();
      }
      TestRunner.evaluateInPage('appendChild()', callback);
    },

    function testAppendWithText(next) {
      function callback() {
        TestRunner.addResult('======== Appended with text=========');
        ElementsTestRunner.dumpElementsTree(containerNode);
        var newNode = ElementsTestRunner.expandedNodeWithId('child-with-text');
        if (TestRunner.domModel.nodeForId(newNode.firstChild.id))
          TestRunner.addResult('Success: child text is bound');
        else
          TestRunner.addResult('Failed: child text is not bound');
        next();
      }
      TestRunner.evaluateInPage('appendChildWithText()', callback);
    },

    function testInsertFirstTextNode(next) {
      function callback() {
        TestRunner.addResult('======== Inserted first text node =========');
        ElementsTestRunner.expandElementsTree(callback2);
      }

      function callback2() {
        ElementsTestRunner.dumpElementsTree(containerNode);
        var newNode = ElementsTestRunner.expandedNodeWithId('child3');
        if (TestRunner.domModel.nodeForId(newNode.firstChild.id))
          TestRunner.addResult('Success: child text is bound');
        else
          TestRunner.addResult('Failed: child text is not bound');
        next();
      }
      TestRunner.evaluateInPage('insertFirstTextNode()', callback);
    }
  ]);
})();
