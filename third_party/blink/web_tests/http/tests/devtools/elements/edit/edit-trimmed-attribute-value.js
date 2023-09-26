// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that user can mutate DOM by means of elements panel.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div>
          <a href="data:text/plain;,12345678901234567890123456789012345678901234567890123456789012345678901234567890/123456789012345678901234567890123456789012345678901234567890" id="inspected">Anchor</a>
      </div>
    `);

  ElementsTestRunner.selectNodeWithId('inspected', execute);

  function execute() {
    var treeElement = ElementsTestRunner.firstElementsTreeOutline().findTreeElement(
        ElementsTestRunner.expandedNodeWithId('inspected'));
    var textElement = treeElement.listItemElement.getElementsByClassName('webkit-html-attribute')[0];
    TestRunner.addResult('Original textContent');
    TestRunner.addResult(treeElement.title.textContent);

    treeElement.startEditingTarget(textElement);
    TestRunner.addResult('textContent when editing \'href\'');
    TestRunner.addResult(treeElement.title.textContent);

    eventSender.keyDown('Tab');
    TestRunner.addResult('textContent after moving to \'id\'');
    TestRunner.addResult(treeElement.title.textContent);

    textElement = treeElement.listItemElement.getElementsByClassName('webkit-html-attribute')[1];
    textElement.dispatchEvent(TestRunner.createKeyEvent('Escape'));
    TestRunner.addResult('textContent after canceling the edit (equal to the original one)');
    TestRunner.addResult(treeElement.title.textContent);

    TestRunner.completeTest();
  }
})();
