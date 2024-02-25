// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as ElementsModule from 'devtools/panels/elements/elements.js';

(async function() {
  TestRunner.addResult(
      `Tests that distributed nodes and their updates are correctly shown in different shadow host display modes.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
<template id="youngestShadowRootTemplate">
    <div class="youngestShadowMain">
        <shadow></shadow>
        <slot name=".distributeMeToYoungest"><div id="fallbackYoungest"></div></slot>
        <div class="innerShadowHost">
            <slot in-youngest-shadow-root="" name=".distributeMeToInner"></slot>
        </div>
    </div>
</template>
<template id="oldestShadowRootTemplate">
    <div class="oldestShadowMain">
        <slot name=".distributeMeToOldest"><div id="fallbackOldest"></div></slot>
    </div>
</template>
<template id="innerShadowRootTemplate">
    <div class="innerShadowMain">
        <slot in-inner-shadow-root="" name=".distributeMeToInner"></slot>
    </div>
</template>
<div id="shadowHost">
    <div slot="distributeMeToYoungest">
        youngest distributed text
    </div>
    <div slot="distributeMeToOldest">
        oldest distributed text
    </div>
    <div slot="distributeMeToInner">
        oldest distributed text
    </div>
    <div slot="distributeMeToInner">
        oldest distributed text
    </div>
</div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function createShadowRootFromTemplate(root, selector, templateId)
      {
          var shadowHost = root.querySelector(selector);
          var shadowRoot = shadowHost.attachShadow({mode: "open"});
          var template = document.querySelector(templateId);
          shadowRoot.appendChild(template.content.cloneNode(true));
          return shadowHost;
      }

      function initOldestShadowRoot()
      {
          createShadowRootFromTemplate(document, "#shadowHost", "#oldestShadowRootTemplate");
      }

      function initYoungestShadowRoot()
      {
          createShadowRootFromTemplate(document, "#shadowHost", "#youngestShadowRootTemplate");
      }

      function initInnerShadowRoot()
      {
          var shadowHost = document.querySelector("#shadowHost");
          var innerShadowHost = createShadowRootFromTemplate(shadowHost.shadowRoot, ".innerShadowHost", "#innerShadowRootTemplate");
          innerShadowHost.id = "innerShadowHost";
      }

      var lastDistributedNodeId = 0;

      function addDistributedNode(oldest)
      {
          var node = document.createElement("div");
          node.slot = oldest ? "distributeMeToOldest" : "distributeMeToYoungest";
          node.slot = "distributeMeAsWell_" + (++lastDistributedNodeId);
          var shadowHost = document.querySelector("#shadowHost");
          shadowHost.appendChild(node);
      }

      function addDistributedNodeToOldest()
      {
          addDistributedNode(true);
      }
  `);

  var shadowHostNode;
  var treeOutline;
  var shadowHostTreeElement;
  var innerShadowHostNode;
  var innerShadowHostTreeElement;

  ElementsTestRunner.expandElementsTree(elementsTreeExpanded);

  function elementsTreeExpanded(node) {
    treeOutline = ElementsTestRunner.firstElementsTreeOutline();
    shadowHostNode = ElementsTestRunner.expandedNodeWithId('shadowHost');
    shadowHostTreeElement = treeOutline.findTreeElement(shadowHostNode);
    expandAndDumpShadowHostNode('========= Original ========', originalElementsTreeDumped);
  }

  function originalElementsTreeDumped(node) {
    TestRunner.evaluateInPage('initOldestShadowRoot()', onOldestShadowRootInitialized);
  }

  function onOldestShadowRootInitialized() {
    expandAndDumpShadowHostNode('========= After shadow root created ========', onOldestShadowRootDumped);
  }

  function onOldestShadowRootDumped() {
    waitForModifiedNodesUpdate('After adding distributed node', distributedNodeChangedAfterFirstAdding);
    TestRunner.evaluateInPage('addDistributedNodeToOldest()');
  }

  function distributedNodeChangedAfterFirstAdding() {
    waitForModifiedNodesUpdate('After adding another distributed node', distributedNodeChangedAfterSecondAdding);
    TestRunner.evaluateInPage('addDistributedNodeToOldest()');
  }

  function distributedNodeChangedAfterSecondAdding() {
    TestRunner.completeTest();
  }

  function waitForModifiedNodesUpdate(title, next) {
    TestRunner.addSniffer(ElementsModule.ElementsTreeOutline.ElementsTreeOutline.prototype, 'updateModifiedNodes', callback);

    function callback() {
      expandAndDumpShadowHostNode('========= ' + title + ' ========', next);
    }
  }

  function expandAndDumpShadowHostNode(title, next) {
    TestRunner.addResult(title);
    ElementsTestRunner.expandElementsTree(callback);

    function callback() {
      ElementsTestRunner.dumpElementsTree(shadowHostNode);
      next();
    }
  }
})();
