// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Elements from 'devtools/panels/elements/elements.js';

(async function() {
  TestRunner.addResult(
      `This test verifies that the correct node is revealed in the DOM tree when asked to reveal a user-agent shadow DOM node.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <p id="description"></p>

      <p id="test1"></p>
    `);
  await TestRunner.evaluateInPagePromise(`
      var input = document.createElement("input");
      input.id = "nested-input";
      input.value = "test";
      test1.attachShadow({mode: 'open'}).appendChild(input);
    `);

  ElementsTestRunner.firstElementsTreeOutline().addEventListener(
      Elements.ElementsTreeOutline.ElementsTreeOutline.Events.SelectedNodeChanged, selectedNodeChanged);

  var nodeChangesRemaining = 2;
  function selectedNodeChanged(event) {
    var node = event.data.node;
    if (node.nodeName() === 'BODY')
      return;
    TestRunner.addResult('SelectedNodeChanged: ' + node.localName() + ' ' + shadowDOMPart(node));
    if (!--nodeChangesRemaining)
      TestRunner.completeTest();
  }

  function shadowDOMPart(node) {
    if (!node.isInShadowTree())
      return '(not in shadow tree)';
    return '(in ' + (node.ancestorUserAgentShadowRoot() ? 'user-agent' : 'author') + ' shadow DOM)';
  }

  ElementsTestRunner.nodeWithId('nested-input', function(node) {
    node.shadowRoots()[0].getChildNodes(childrenCallback);

    function childrenCallback(children) {
      var shadowDiv = children[0];
      TestRunner.addResult('User-agent shadow DOM hidden:');
      Elements.ElementsPanel.ElementsPanel.instance().revealAndSelectNode(shadowDiv).then(() => {
        Common.Settings.settingForTest('show-ua-shadow-dom').set(true);
        TestRunner.addResult('User-agent shadow DOM shown:');
        Elements.ElementsPanel.ElementsPanel.instance().revealAndSelectNode(shadowDiv);
      });
    }
  });
})();
