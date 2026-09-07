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

  function dumpElementsTree() {
    ElementsTestRunner.dumpElementsTree(null, 0);
  }

  const node = await new Promise(
      resolve => ElementsTestRunner.nodeWithId('data', resolve));
  const dataTreeElement =
      ElementsTestRunner.firstElementsTreeOutline().findTreeElement(node);
  dataTreeElement.expandedChildrenLimitInternal = 5;
  dataTreeElement.reveal();

  await dataTreeElement.onpopulate();
  dataTreeElement.expand();
  ElementsTestRunner.firstElementsTreeOutline().runPendingUpdates();

  TestRunner.addResult('=========== Loaded 5 children ===========');
  dumpElementsTree();

  const updatePromise = TestRunner.addSnifferPromise(
      ElementsModule.ElementsTreeOutline.ElementsTreeOutline.prototype,
      'updateModifiedNodes');
  TestRunner.evaluateInPage('insertNode()');
  await updatePromise;

  TestRunner.addResult('=========== Modified children ===========');
  dumpElementsTree();

  dataTreeElement.expandAllButtonElement.button.click();
  ElementsTestRunner.firstElementsTreeOutline().runPendingUpdates();

  TestRunner.addResult('=========== Loaded all children ===========');
  dumpElementsTree();
  TestRunner.completeTest();
})();
