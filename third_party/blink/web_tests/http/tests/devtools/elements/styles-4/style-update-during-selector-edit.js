// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Elements from 'devtools/panels/elements/elements.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that modification of element styles while editing a selector does not commit the editor.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      .new-class {
          color: red;
      }
      </style>
      <div id="inspected"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function addStyleClass()
      {
          document.getElementById("inspected").className = "new-class";
      }
  `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);
  var treeOutline = ElementsTestRunner.firstElementsTreeOutline();
  var seenRebuildUpdate;
  var seenAttrModified;
  var modifiedAttrNodes = [];

  function attributeChanged(event) {
    if (event.data.node === treeOutline.selectedDOMNode())
      seenAttrModified = true;
  }

  function rebuildUpdate() {
    if (Elements.ElementsPanel.ElementsPanel.instance().stylesWidget.node === treeOutline.selectedDOMNode())
      seenRebuildUpdate = true;
  }

  function step1() {
    TestRunner.addSniffer(Elements.StylesSidebarPane.StylesSidebarPane.prototype, 'doUpdate', rebuildUpdate);
    TestRunner.domModel.addEventListener(SDK.DOMModel.Events.AttrModified, attributeChanged, this);
    // Click "Add new rule".
    Elements.ElementsPanel.ElementsPanel.instance()
        .stylesWidget.contentElement.querySelector('.styles-pane-toolbar')
        .shadowRoot.querySelector('[aria-label="New Style Rule"]')
        .click();
    TestRunner.evaluateInPage('addStyleClass()', step2);
  }

  function step2() {
    if (!seenAttrModified)
      TestRunner.addResult('FAIL: AttrModified event not received.');
    else if (seenRebuildUpdate)
      TestRunner.addResult('FAIL: Styles pane updated while a selector editor was active.');
    else
      TestRunner.addResult('SUCCESS: Styles pane not updated.');
    TestRunner.completeTest();
  }
})();
