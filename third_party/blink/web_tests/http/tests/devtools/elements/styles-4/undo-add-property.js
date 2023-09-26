// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that adding a property is undone properly.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      .container {
        font-weight: bold
      }
      </style>
      <div id="container" class="container"></div>
      <div id="other" class="container"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('container', testAppendProperty);

  async function testAppendProperty() {
    TestRunner.addResult('=== Last property ===');
    await testAddProperty('margin-left: 2px;', undefined, testInsertBegin);
  }

  async function testInsertBegin() {
    TestRunner.addResult('=== First property ===');
    await testAddProperty('margin-top: 0px;', 0, testInsertMiddle);
  }

  async function testInsertMiddle() {
    TestRunner.addResult('=== Middle property ===');
    await testAddProperty('margin-right: 1px;', 1, TestRunner.completeTest.bind(TestRunner));
  }

  async function testAddProperty(propertyText, index, callback) {
    TestRunner.addResult('(Initial value)');
    await ElementsTestRunner.dumpSelectedElementStyles(true);

    var treeItem = ElementsTestRunner.getMatchedStylePropertyTreeItem('font-weight');
    var treeElement = treeItem.section().addNewBlankProperty(index);
    treeElement.applyStyleText(propertyText, true);
    ElementsTestRunner.waitForStyles('container', step1);

    async function step1() {
      TestRunner.addResult('(After adding property)');
      await ElementsTestRunner.dumpSelectedElementStyles(true);

      SDK.DOMModel.DOMModelUndoStack.instance().undo();
      ElementsTestRunner.selectNodeAndWaitForStyles('other', step2);
    }

    async function step2() {
      TestRunner.addResult('(After undo)');
      await ElementsTestRunner.dumpSelectedElementStyles(true);

      SDK.DOMModel.DOMModelUndoStack.instance().redo();
      ElementsTestRunner.selectNodeAndWaitForStyles('container', step3);
    }

    async function step3() {
      TestRunner.addResult('(After redo)');
      await ElementsTestRunner.dumpSelectedElementStyles(true);
      callback();
    }
  }
})();
