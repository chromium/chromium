// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`This test checks dom extensions.\n`);


  TestRunner.runTestSuite([
    function traverseNextNodeInShadowDom(next) {
      function createSlot(parent, name) {
        const slot = parent.createChild('slot');
        if (name)
          slot.name = name;
        return slot;
      }

      function createChild(parent, tagName, name, text = '') {
        const child = parent.createChild(tagName, name);
        if (name)
          child.slot = name;
        child.textContent = text;
        return child;
      }

      var component1 = document.createElement('div');
      component1.classList.add('component1');
      var shadow1 = component1.attachShadow({mode: 'open'});
      createChild(component1, 'div', 'component1-content', 'text 1');
      createChild(component1, 'div', 'component2-content', 'text 2');
      createChild(component1, 'span', undefined, 'text 3');
      createChild(component1, 'span', 'component1-content', 'text 4');

      var shadow1Content = document.createElement('div');
      shadow1Content.classList.add('shadow-component1');
      shadow1.appendChild(shadow1Content);
      createSlot(shadow1Content, 'component1-content');
      createSlot(shadow1Content);

      var component2 = shadow1Content.createChild('div', 'component2');
      var shadow2 = component2.attachShadow({mode: 'open'});
      createSlot(component2, 'component2-content');
      createChild(
          component2, 'div', 'component2-content', 'component2 light dom text');

      var shadow2Content = document.createElement('div');
      shadow2Content.classList.add('shadow-component1');
      shadow2.appendChild(shadow2Content);
      var midDiv = createChild(shadow2Content, 'div', 'mid-div');
      createChild(midDiv, 'div', undefined, 'component2-text');
      createSlot(midDiv);
      createSlot(midDiv, 'component2-content');

      var node = component1;
      while ((node = node.traverseNextNode(component1))) {
        if (node.nodeType === Node.TEXT_NODE)
          TestRunner.addResult(node.nodeValue);
        else
          TestRunner.addResult(node.nodeName + (node.className ? '.' + node.className : ''));
      }
      next();
    },
  ]);
})();
