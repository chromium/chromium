// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as ElementsModule from 'devtools/panels/elements/elements.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that nested pseudo elements and their styles are handled properly.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #inspected::before {
        content: "BEFORE";
      }

      #inspected::after {
        content: "AFTER";
      }

      #inspected::before {
        display: list-item;
      }

      #inspected::after {
        display: list-item;
      }
      </style>
      <div id="container">
        <div id="inspected">Text</div>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function removeLastRule()
      {
          document.styleSheets[0].deleteRule(document.styleSheets[0].cssRules.length - 1);
      }

      function addAfterMarkerRule()
      {
          document.styleSheets[0].addRule("#inspected::after", "display: list-item");
      }

      function addBeforeMarkerRule()
      {
          document.styleSheets[0].addRule("#inspected::before", "display: list-item");
      }

      function removeNode()
      {
          document.getElementById("inspected").remove();
      }
  `);

  var containerNode;
  var inspectedNode;

  TestRunner.runTestSuite([
    function dumpOriginalTree(next) {
      ElementsTestRunner.expandElementsTree(callback);
      function callback() {
        containerNode = ElementsTestRunner.expandedNodeWithId('container');
        inspectedNode = ElementsTestRunner.expandedNodeWithId('inspected');
        TestRunner.addResult('Original elements tree:');
        ElementsTestRunner.dumpElementsTree(containerNode);
        next();
      }
    },

    function dumpBeforeStyles(next) {
      selectNodeAndDumpStyles('inspected', ['before'], next);
    },

    function dumpAfterStyles(next) {
      selectNodeAndDumpStyles('inspected', ['after'], next);
    },

    function dumpBeforeMarkerStyles(next) {
      selectNodeAndDumpStyles('inspected', ['before', 'marker'], next);
    },

    function dumpAfterMarkerStyles(next) {
      selectNodeAndDumpStyles('inspected', ['after', 'marker'], next);
    },

    function removeAfterMarker(next) {
      executeAndDumpTree('removeLastRule()', SDK.DOMModel.Events.NodeRemoved, next);
    },

    function removeBeforeMarker(next) {
      executeAndDumpTree('removeLastRule()', SDK.DOMModel.Events.NodeRemoved, next);
    },

    function addAfterMarker(next) {
      executeAndDumpTree('addAfterMarkerRule()', SDK.DOMModel.Events.NodeInserted, expandAndDumpTree.bind(this, next));
    },

    function addBeforeMarker(next) {
      executeAndDumpTree('addBeforeMarkerRule()', SDK.DOMModel.Events.NodeInserted, next);
    },

    function removeNodeAndCheckPseudoElementsUnbound(next) {
      var inspectedBefore = inspectedNode.beforePseudoElement();
      var inspectedBeforeMarker = inspectedBefore.markerPseudoElement();
      var inspectedAfter = inspectedNode.afterPseudoElement();
      var inspectedAfterMarker = inspectedAfter.markerPseudoElement();

      executeAndDumpTree('removeNode()', SDK.DOMModel.Events.NodeRemoved, callback);
      function callback() {
        TestRunner.addResult(
            'inspected::before DOMNode in DOMAgent: ' + !!(TestRunner.domModel.nodeForId(inspectedBefore.id)));
        TestRunner.addResult(
            'inspected::before::marker DOMNode in DOMAgent: ' + !!(TestRunner.domModel.nodeForId(inspectedBeforeMarker.id)));
        TestRunner.addResult(
            'inspected::after DOMNode in DOMAgent: ' + !!(TestRunner.domModel.nodeForId(inspectedAfter.id)));
        TestRunner.addResult(
            'inspected::after::marker DOMNode in DOMAgent: ' + !!(TestRunner.domModel.nodeForId(inspectedAfterMarker.id)));
        next();
      }
    }
  ]);

  function executeAndDumpTree(pageFunction, eventName, next) {
    TestRunner.domModel.addEventListener(eventName, domCallback, this);
    TestRunner.evaluateInPage(pageFunction);

    function domCallback() {
      TestRunner.domModel.removeEventListener(eventName, domCallback, this);
      ElementsTestRunner.firstElementsTreeOutline().addEventListener(
          ElementsModule.ElementsTreeOutline.ElementsTreeOutline.Events.ElementsTreeUpdated, treeCallback, this);
    }

    function treeCallback() {
      ElementsTestRunner.firstElementsTreeOutline().removeEventListener(
          ElementsModule.ElementsTreeOutline.ElementsTreeOutline.Events.ElementsTreeUpdated, treeCallback, this);
      ElementsTestRunner.dumpElementsTree(containerNode);
      next();
    }
  }

  function expandAndDumpTree(next) {
    TestRunner.addResult('== Expanding: ==');
    ElementsTestRunner.expandElementsTree(callback);
    function callback() {
      ElementsTestRunner.dumpElementsTree(containerNode);
      next();
    }
  }

  function selectPseudoElementAndWaitForStyles(parentId, pseudoTypes, callback) {
    if (!pseudoTypes.length) {
      ElementsTestRunner.selectNodeAndWaitForStyles(parentId, callback);
      return;
    }

    pseudoTypes.reduce(async function(prev, pseudoType) {
      let prevNode = await prev;
      function isCurrentPseudoElement(node) {
        if (node.pseudoType() !== pseudoType)
          return false;
        const {parentNode} = node;
        if (!parentNode)
          return false;
        if (prevNode)
          return parentNode === prevNode;
        return parentNode.getAttribute('id') == parentId;
      }
      let stylesUpdated = new Promise((resolve) => {
        waitForStylesRebuild(isCurrentPseudoElement, resolve, true);
      });
      let node = await new Promise((resolve) => {
        ElementsTestRunner.findNode(isCurrentPseudoElement, resolve);
      });
      if (!node)
        throw new Error("Can't find node");
      Common.Revealer.reveal(node);
      await stylesUpdated;
      return node;
    }, null).then(callback);
  }

  function selectNodeAndDumpStyles(id, pseudoTypeNames, callback) {
    selectPseudoElementAndWaitForStyles(id, pseudoTypeNames, stylesCallback);

    async function stylesCallback() {
      await ElementsTestRunner.dumpSelectedElementStyles(true, false, false, true);
      callback();
    }
  }
})();
