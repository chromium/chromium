// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Platform from 'devtools/core/platform/platform.js';
import * as ElementsModule from 'devtools/panels/elements/elements.js';
import * as UIModule from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult('Tests that styles sidebar can be navigated with arrow keys.\n');

  await TestRunner.showPanel('elements');

  await TestRunner.loadHTML(`
    <style>
    #foo {
      color: red;
    }
    div {
      color: blue;
    }
    div#foo {
      color: green;
    }
    </style>
    <div id="foo"></div>
  `);
  ElementsTestRunner.selectNodeWithId('foo');

  await waitForStylesRebuild();

  let ssp = ElementsModule.ElementsPanel.ElementsPanel.instance().stylesWidget;

  // start editing
  ssp.sectionBlocks[0].sections[0].element.focus();
  ssp.sectionBlocks[0].sections[0].addNewBlankProperty(0).startEditingName();

  dumpState();

  TestRunner.addResult("Pressing escape to stop editing");
  eventSender.keyDown("Escape");
  dumpState()

  TestRunner.addResult("Navigating with arrow keys");
  eventSender.keyDown("ArrowDown");
  dumpState()

  eventSender.keyDown("ArrowDown");
  dumpState()

  eventSender.keyDown("ArrowDown");
  dumpState()

  eventSender.keyDown("ArrowDown");
  dumpState()

  eventSender.keyDown("ArrowUp");
  dumpState()

  TestRunner.addResult("Typing should start editing");
  eventSender.keyDown("t");
  dumpState()

  TestRunner.addResult("Pressing escape again should restore focus");
  eventSender.keyDown("Escape");
  dumpState()

  TestRunner.addResult("Pressing enter should start editing selector");
  eventSender.keyDown("Enter");
  dumpState()

  TestRunner.completeTest();


  function dumpState() {
    TestRunner.addResult('Editing: ' + UIModule.UIUtils.isEditing())
    TestRunner.addResult(Platform.DOMUtilities.deepActiveElement(document).textContent);
    TestRunner.addResult('');
  }

  function waitForStylesRebuild(node) {
    if (node && node.getAttribute("id") === 'foo')
      return;
    return TestRunner.addSnifferPromise(ElementsModule.StylesSidebarPane.StylesSidebarPane.prototype, "nodeStylesUpdatedForTest").then(waitForStylesRebuild);
  }



})();
