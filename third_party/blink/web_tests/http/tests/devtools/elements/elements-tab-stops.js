// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Platform from 'devtools/core/platform/platform.js';

(async function() {
  TestRunner.addResult(`Tests what elements have focus after pressing tab.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <span id="node-to-select"></span>
    `);


  var maxElements = 1000;
  var elements = 0;

  // Make sure the sidebar is loaded

  // make sure the tree is loaded
  ElementsTestRunner.selectNodeAndWaitForStyles('node-to-select', nodeSelected);
  function nodeSelected() {
    eventSender.keyDown('Tab');
    var startElement = Platform.DOMUtilities.deepActiveElement(document);
    do {
      dumpFocus();
      eventSender.keyDown('Tab');
      elements++;
    } while (startElement !== Platform.DOMUtilities.deepActiveElement(document) && elements < maxElements);

    TestRunner.addResult('');
    TestRunner.addResult('Shift+Tab:');
    TestRunner.addResult('');
    startElement = Platform.DOMUtilities.deepActiveElement(document);
    do {
      dumpFocus();
      eventSender.keyDown('Tab', ['shiftKey']);
      elements++;
    } while (startElement !== Platform.DOMUtilities.deepActiveElement(document) && elements < maxElements);

    if (elements >= maxElements)
      TestRunner.addResult('FAIL: Unable to complete tab stop cycle.');

    TestRunner.completeTest();
  }

  function dumpFocus() {
    var element = Platform.DOMUtilities.deepActiveElement(document);
    if (!element) {
      TestRunner.addResult('null');
      return;
    }
    var name = element.tagName;
    if (element.id)
      name += '#' + element.id;
    if (element.getAttribute('aria-label'))
      name += ':' + element.getAttribute('aria-label');
    else if (element.title)
      name += ':' + element.title;
    else if (element.textContent && element.textContent.length < 50) {
      name += ':' + element.textContent.replace('\u200B', '');
    } else if (element.className)
      name += '.' + element.className.split(' ').join('.');
    TestRunner.addResult(name);
  }
})();
