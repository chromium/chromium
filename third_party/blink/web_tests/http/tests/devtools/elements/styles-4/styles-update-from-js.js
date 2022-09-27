// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that changes to an inline style and ancestor/sibling className from JavaScript are reflected in the Styles pane and Elements tree.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
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

  TestRunner.runTestSuite([
    function testInit(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('container', next);
    },

    function testSetStyleAttribute(next) {
      waitAndDumpAttributeAndStyles(next);
      TestRunner.evaluateInPage('modifyStyleAttribute()');
    },

    function testSetStyleCSSText(next) {
      waitAndDumpAttributeAndStyles(next);
      TestRunner.evaluateInPage('modifyCSSText()');
    },

    function testSetViaParsedAttributes(next) {
      waitAndDumpAttributeAndStyles(next);
      TestRunner.evaluateInPage('modifyParsedAttributes()');
    },

    function testSetViaAncestorClass(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('child', callback);

      function callback() {
        waitAndDumpAttributeAndStyles(next, 'child');
        TestRunner.evaluateInPage('modifyContainerClass()');
      }
    },

    function testSetViaSiblingAttr(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('childSibling', callback);

      function callback() {
        waitAndDumpAttributeAndStyles(next, 'childSibling');
        TestRunner.evaluateInPage('modifyChildAttr()');
      }
    }
  ]);

  function waitAndDumpAttributeAndStyles(next, id) {
    id = id || 'container';
    async function callback() {
      await dumpAttributeAndStyles(id);
      next();
    }
    ElementsTestRunner.waitForStyles(id, callback);
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
