// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as ElementsModule from 'devtools/panels/elements/elements.js';

(async function() {
  TestRunner.addResult(`Test that Web Inspector can inspect element with pointer-events:none.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      div {
          margin: 0;
          padding: 0;
          border: none;
      }
      #outer {
          position: absolute;
          top: 0;
          left: 0;
          bottom: 0;
          right: 0;
      }
      #inner {
          pointer-events: none;
          position: absolute;
          top: 10px;
          left: 10px;
          bottom: 10px;
          right: 10px;
      }
      </style>
      <div id="outer"><div id="inner"></div></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function clickInner(withShift)
      {
          var target = document.getElementById("inner");
          var rect = target.getBoundingClientRect();
          // Simulate the mouse click over the target to trigger an event dispatch.
          if (window.eventSender) {
              eventSender.mouseMoveTo(rect.left + rect.width / 2, rect.top + rect.height / 2, withShift ? "shiftKey" : "");
              eventSender.mouseDown();
              eventSender.mouseUp();
          }
      }
  `);

  function selectedNodeId() {
    var selectedElement = ElementsTestRunner.firstElementsTreeOutline().selectedTreeElement;
    if (!selectedElement)
      return '<no selected node>';
    return selectedElement.node().getAttribute('id');
  }

  function expectSelectedNode(expectedId) {
    var id = selectedNodeId();
    if (id === expectedId)
      TestRunner.addResult('PASS: selected node with id \'' + id + '\'');
    else
      TestRunner.addResult('FAIL: unexpected selection ' + id);
  }

  function step1() {
    TestRunner.overlayModel.setInspectMode(Protocol.Overlay.InspectMode.SearchForNode).then(step2);
  }

  function step2() {
    ElementsTestRunner.firstElementsTreeOutline().addEventListener(
        ElementsModule.ElementsTreeOutline.ElementsTreeOutline.Events.SelectedNodeChanged, step3);
    TestRunner.evaluateInPage('clickInner(true)');
  }

  function step3() {
    ElementsTestRunner.firstElementsTreeOutline().removeEventListener(
        ElementsModule.ElementsTreeOutline.ElementsTreeOutline.Events.SelectedNodeChanged, step3);
    expectSelectedNode('inner');
    TestRunner.overlayModel.setInspectMode(Protocol.Overlay.InspectMode.SearchForNode).then(step4);
  }

  function step4() {
    ElementsTestRunner.firstElementsTreeOutline().addEventListener(
        ElementsModule.ElementsTreeOutline.ElementsTreeOutline.Events.SelectedNodeChanged, step5);
    TestRunner.evaluateInPage('clickInner(false)');
  }

  function step5() {
    ElementsTestRunner.firstElementsTreeOutline().removeEventListener(
        ElementsModule.ElementsTreeOutline.ElementsTreeOutline.Events.SelectedNodeChanged, step5);
    expectSelectedNode('outer');
    TestRunner.completeTest();
  }

  step1();
})();
