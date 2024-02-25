// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Verifies simple selector behavior.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container" style="display: none">
          <div id="foo"></div>
          <div id="bar" class="foo"></div>
          <div class="title"></div>
          <input>
          <input type="text">
          <input type="text" class="text-input">
          <input type="text" id="text-input">
          <p class="article"></p>
          <span></span>
          <b></b>
      </div>
    `);

  ElementsTestRunner.expandElementsTree(onElementsTreeExpanded);

  function onElementsTreeExpanded() {
    var container = ElementsTestRunner.expandedNodeWithId('container');
    var children = container.children();
    for (var i = 0; i < children.length; ++i)
      TestRunner.addResult(children[i].simpleSelector());
    TestRunner.completeTest();
  }
})();
