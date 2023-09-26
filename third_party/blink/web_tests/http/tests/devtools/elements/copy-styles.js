// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as ElementsModule from 'devtools/panels/elements/elements.js';

(async function() {
  TestRunner.addResult(`Tests node xPath construction\n`);
  await TestRunner.showPanel('elements');

  await TestRunner.loadHTML(`
    <style>
      button {
        color: red;
      }
      body {
        border: 1px solid black;
        font-weight: bold;
      }
    </style>
    <div style="padding: 5px"> Hello </div>
    <button> A red button </button>
    <button style="color: green"> A green button </button>

  `);
  InspectorFrontendHost.copyText = text => {
    TestRunner.addResult('InspectorFrontendHost.copyText:\n' + text)
  };

  await new Promise(x => ElementsTestRunner.expandElementsTree(x));


  const element = ElementsTestRunner.firstElementsTreeOutline().rootElement();

  await processElement(element);

  TestRunner.completeTest();

  async function processElement(element) {
    if (element instanceof ElementsModule.ElementsTreeElement.ElementsTreeElement && !element.isClosingTag() && element.node().nodeNameInCorrectCase() !== 'base') {
      TestRunner.addResult('\n' + element.listItemElement.textContent);
      await element.copyStyles();
    }
    for (const child of element.children()) {
      await processElement(child);
    }
  }


})();
