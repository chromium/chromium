// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that elements hidden by "Show more" limit are revealed properly.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <div>
      <div id="data">
        <div><span id="id1">1</span></div>
        <div><span id="id2">2</span></div>
        <div><span id="id3">3</span></div>
        <div><span id="id4">4</span></div>
        <div><span id="id5">5</span></div>
        <div><span id="id6">6</span></div>
        <div><span id="id7">7</span></div>
        <div><span id="id8">8</span></div>
        <div><span id="id9">9</span></div>
        <div><span id="id10">10</span></div>
      </div>
    </div>
    `);

  const containerNode = await ElementsTestRunner.nodeWithIdPromise('data');
  var containerTreeElement = ElementsTestRunner.firstElementsTreeOutline().findTreeElement(containerNode);
  containerTreeElement.expandedChildrenLimitInternal = 5;
  containerTreeElement.reveal();
  containerTreeElement.expand();
  TestRunner.deprecatedRunAfterPendingDispatches(step2);

  async function step2() {
    TestRunner.addResult('=========== Loaded 5 children ===========');
    dumpElementsTree();

    TestRunner.addResult('=========== Revealing nested node behind "Show more" ===========');
    const hiddenNode = await ElementsTestRunner.nodeWithIdPromise('id10');
    await ElementsTestRunner.selectNode(hiddenNode);
    dumpElementsTree();
    TestRunner.completeTest();
  }

  function dumpElementsTree() {
    ElementsTestRunner.dumpElementsTree(null, 0);
  }
})();
