// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Platform from 'devtools/core/platform/platform.js';
import * as Elements from 'devtools/panels/elements/elements.js';

(async function() {
  TestRunner.addResult(`Tests that the styles sidebar can be used with a mouse.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <style>
      #inspected {
        color: blue;
        background-color: red;
      }
    </style>
    <div id="inspected">Text</div>
  `);
  await new Promise(x => ElementsTestRunner.selectNodeAndWaitForStyles('inspected', x));

  var stylesPane = Elements.ElementsPanel.ElementsPanel.instance().stylesWidget;
  var firstRule = stylesPane.sectionBlocks[0].sections[1].propertiesTreeOutline;
  var blueElement = () => firstRule.firstChild().valueElement;
  var colorElement = () => firstRule.firstChild().nameElement;
  var listItemElement = () => firstRule.firstChild().listItemElement;

  TestRunner.addResult('Test switching between items')
  mouseDown(blueElement());
  mouseUp(blueElement());
  TestRunner.addResult('');

  mouseDown(colorElement());
  mouseUp(colorElement());
  TestRunner.addResult('');

  mouseDown(blueElement());
  mouseUp(colorElement());
  TestRunner.addResult('');

  TestRunner.addResult('Cancel editing by clicking a blank area');
  mouseDown(listItemElement(), 6); // offset the click to stop eventSender from doing dblclick
  mouseUp(listItemElement(), 6);
  TestRunner.addResult('');

  TestRunner.addResult('Create a new property by clicking a blank area');
  mouseDown(listItemElement());
  mouseUp(listItemElement());
  TestRunner.addResult('');

  TestRunner.addResult('Test disabling the property');

  var checkbox = () => listItemElement().querySelector('.enabled-button');
  mouseDown(checkbox());
  mouseUp(checkbox());
  TestRunner.addResult('Enabled: ' + checkbox().checked);
  TestRunner.addResult('');


  TestRunner.addResult('Test enabling the property');
  mouseDown(checkbox());
  mouseUp(checkbox());
  TestRunner.addResult('Enabled: ' + checkbox().checked);
  TestRunner.addResult('');


  TestRunner.completeTest();

  function dumpEditingState() {
    if (!stylesPane.isEditingStyle) {
      TestRunner.addResult('Not editing');
      return;
    }
    TestRunner.addResult('Editing: "' + TestRunner.textContentWithoutStyles(Platform.DOMUtilities.deepActiveElement(document)) + '"');
  }

  function mouseDown(element, offset = 0) {
    TestRunner.addResult('mouse down: ' + element.tagName + ':' + TestRunner.textContentWithoutStyles(element));
    var rect = element.getBoundingClientRect();
    eventSender.mouseMoveTo((rect.left + rect.right) / 2 + offset, (rect.top + rect.bottom) / 2);
    eventSender.mouseDown();
    dumpEditingState();
  }

  function mouseUp(element, offset = 0) {
    TestRunner.addResult('mouse up: ' + element.tagName + ':' + TestRunner.textContentWithoutStyles(element));
    var rect = element.getBoundingClientRect();
    eventSender.mouseMoveTo((rect.left + rect.right) / 2 + offset, (rect.top + rect.bottom) / 2);
    eventSender.mouseUp();
    dumpEditingState();
  }

})();
