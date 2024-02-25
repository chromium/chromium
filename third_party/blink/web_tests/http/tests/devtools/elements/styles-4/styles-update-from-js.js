// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that changes to an inline style and ancestor/sibling className from JavaScript are reflected in the Styles pane and Elements tree.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      .red div:first-child {
          background-color: red;
      }

      div[foo="bar"] + div {
          background-color: blue;
      }

      </style>
      <div id="container" style="font-weight:bold"><div id="child"></div><div id="childSibling"></div></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function modifyStyleAttribute()
      {
          document.getElementById("container").setAttribute("style", "color: #daC0DE; border: 1px solid black;");
      }

      function modifyCSSText()
      {
          document.getElementById("container").style.cssText = "color: #C0FFEE";
      }

      function modifyParsedAttributes()
      {
          var style = document.getElementById("container").style;
          style.border = "2px dashed green";
          style.borderWidth = "3px";
      }

      function modifyContainerClass()
      {
          document.getElementById("container").className = "red";
      }

      function modifyChildAttr()
      {
          document.getElementById("child").setAttribute("foo", "bar");
      }
  `);

  TestRunner.runAsyncTestSuite([
    async function testInit(next) {
      await ElementsTestRunner.selectNodeAndWaitForStylesPromise('container');
    },

    async function testSetStyleAttribute() {
      await TestRunner.evaluateInPage('modifyStyleAttribute()');
      await waitAndDumpAttributeAndStyles();
    },

    async function testSetStyleCSSText() {
      await TestRunner.evaluateInPage('modifyCSSText()');
      await waitAndDumpAttributeAndStyles();
    },

    async function testSetViaParsedAttributes() {
      await TestRunner.evaluateInPage('modifyParsedAttributes()');
      await waitAndDumpAttributeAndStyles();
    },

    async function testSetViaAncestorClass() {
      await ElementsTestRunner.selectNodeAndWaitForStylesPromise('child');
      await TestRunner.evaluateInPage('modifyContainerClass()');
      await waitAndDumpAttributeAndStyles('child');
    },

    async function testSetViaSiblingAttr() {
      await ElementsTestRunner.selectNodeAndWaitForStylesPromise('childSibling');
      await TestRunner.evaluateInPage('modifyChildAttr()');
      await waitAndDumpAttributeAndStyles('childSibling');
    }
  ]);

  async function waitAndDumpAttributeAndStyles(id) {
    id = id || 'container';
    await new Promise(resolve => ElementsTestRunner.waitForStyles(id, resolve));
    await dumpAttributeAndStyles(id);
  }

  async function dumpAttributeAndStyles(id) {
    var treeElement = findNodeTreeElement(id);
    if (!treeElement) {
      TestRunner.addResult('\'' + id + '\' tree element not found');
      return;
    }
    TestRunner.addResult(treeElement.listItemElement.textContent.replace(/\u200b/g, ''));
    await ElementsTestRunner.dumpSelectedElementStyles(true);
  }

  function findNodeTreeElement(id) {
    ElementsTestRunner.firstElementsTreeOutline().runPendingUpdates();
    var expandedNode = ElementsTestRunner.expandedNodeWithId(id);
    if (!expandedNode) {
      TestRunner.addResult('\'' + id + '\' node not found');
      TestRunner.completeTest();
    }
    return ElementsTestRunner.firstElementsTreeOutline().findTreeElement(expandedNode);
  }
})();
