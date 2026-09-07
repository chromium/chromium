// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ConsoleTestRunner} from 'console_test_runner';
import * as ElementsModule from 'devtools/panels/elements/elements.js';
import {ElementsTestRunner} from 'elements_test_runner';
import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(
      `Test that web inspector can select element in an iframe even if the element was created via createElement of document other than iframe's document. Bug 60031\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <iframe style="width:400px"></iframe>
    `);
  await TestRunner.evaluateInPagePromise(`
      var el1;
      var el2;
      function createDynamicElements()
      {
          var mainDoc = document;
          var frameDoc = window.frames[0].document;

          el1 = mainDoc.createElement('div');
          el1.id = "main-frame-div";
          el2 = frameDoc.createElement('div');
          el2.id = "iframe-div";

          el1.innerHTML = 'Element created via &lt;main document>.createElement';
          el2.innerHTML = 'Element created via &lt;frame document>.createElement';

          frameDoc.body.appendChild(el1);
          frameDoc.body.appendChild(el2);
      }
  `);

  await TestRunner.evaluateInPagePromise('createDynamicElements()');

  function selectedNodeId() {
    var selectedElement = ElementsTestRunner.firstElementsTreeOutline().selectedTreeElement;
    if (!selectedElement)
      return '<no selected node>';
    return selectedElement.node().getAttribute('id');
  }

  function waitForSelectedNode(predicate) {
    const treeOutline = ElementsTestRunner.firstElementsTreeOutline();
    const currentNode = treeOutline.selectedDOMNode();
    if (currentNode && predicate(currentNode)) {
      return Promise.resolve(currentNode);
    }
    return new Promise(resolve => {
      function listener(event) {
        const node = event.data.node;
        if (node && predicate(node)) {
          treeOutline.removeEventListener(
              ElementsModule.ElementsTreeOutline.ElementsTreeOutline.Events
                  .SelectedNodeChanged,
              listener);
          resolve(node);
        }
      }
      treeOutline.addEventListener(
          ElementsModule.ElementsTreeOutline.ElementsTreeOutline.Events
              .SelectedNodeChanged,
          listener);
    });
  }

  let selectedPromise =
      waitForSelectedNode(node => node.getAttribute('id') === 'main-frame-div');
  await ConsoleTestRunner.evaluateInConsolePromise('inspect(el1)');
  await selectedPromise;
  ElementsTestRunner.firstElementsTreeOutline().runPendingUpdates();
  let id = selectedNodeId();
  if (id === 'main-frame-div')
    TestRunner.addResult('PASS: selected node  with id \'' + id + '\'');
  else
    TestRunner.addResult('FAIL: unexpected selection ' + id);

  // Frame was changed to the iframe. Moving back to the top frame.
  selectedPromise = waitForSelectedNode(node => node.nodeName() === 'BODY');
  await ConsoleTestRunner.evaluateInConsolePromise(
      'inspect(window.frameElement.parentElement)',
      /* dontForceMainContext= */ true);
  await selectedPromise;
  ElementsTestRunner.firstElementsTreeOutline().runPendingUpdates();

  selectedPromise =
      waitForSelectedNode(node => node.getAttribute('id') === 'iframe-div');
  await ConsoleTestRunner.evaluateInConsolePromise('inspect(el2)');
  await selectedPromise;
  ElementsTestRunner.firstElementsTreeOutline().runPendingUpdates();
  id = selectedNodeId();
  if (id === 'iframe-div')
    TestRunner.addResult('PASS: selected node  with id \'' + id + '\'');
  else
    TestRunner.addResult('FAIL: unexpected selection ' + id);

  TestRunner.completeTest();
})();
