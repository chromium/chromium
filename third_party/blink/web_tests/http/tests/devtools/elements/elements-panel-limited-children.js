// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as ElementsModule from 'devtools/panels/elements/elements.js';

(async function() {
  TestRunner.addResult(`Tests that src and href element targets are rewritten properly.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="data">
      <div id="id1">1</div>
      <div id="id2">2</div>
      <div id="id3">3</div>
      <div id="id4">4</div>
      <div id="id5">5</div>
      <div id="id6">6</div>
      <div id="id7">7</div>
      <div id="id8">8</div>
      <div id="id9">9</div>
      <div id="id10">10</div>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function insertNode()
      {
          var dataElement = document.getElementById("data");
          dataElement.appendChild(document.createElement("a"));
          dataElement.removeChild(document.getElementById("id2"));
          var aElement = document.createElement("a");
          dataElement.insertBefore(aElement, document.getElementById("id1"));
          dataElement.appendChild(aElement);
          dataElement.insertBefore(aElement, document.getElementById("id1"));
      }
  `);

  var dataTreeElement;
  ElementsTestRunner.nodeWithId('data', step1);

  function step1(node) {
    dataTreeElement = ElementsTestRunner.firstElementsTreeOutline().findTreeElement(node);
    dataTreeElement.expandedChildrenLimitInternal = 5;
    dataTreeElement.reveal();
    dataTreeElement.expand();
    TestRunner.deprecatedRunAfterPendingDispatches(step2);
  }

  function step2() {
    TestRunner.addResult('=========== Loaded 5 children ===========');
    dumpElementsTree();
    TestRunner.addSniffer(ElementsModule.ElementsTreeOutline.ElementsTreeOutline.prototype, 'updateModifiedNodes', step3);
    TestRunner.evaluateInPage('insertNode()');
  }

  function step3() {
    TestRunner.addResult('=========== Modified children ===========');
    dumpElementsTree();
    dataTreeElement.expandAllButtonElement.button.click();
    TestRunner.deprecatedRunAfterPendingDispatches(step4);
  }

  function step4() {
    TestRunner.addResult('=========== Loaded all children ===========');
    dumpElementsTree();
    TestRunner.completeTest();
  }

  function dumpElementsTree() {
    ElementsTestRunner.dumpElementsTree(null, 0);
  }
})();
