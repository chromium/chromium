// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Elements from 'devtools/panels/elements/elements.js';

(async function() {
  TestRunner.addResult(
      `Tests that content-box and border-box content area dimensions are handled property by the Metrics pane.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #border-box {
          box-sizing: border-box;
          width: 55px;
          height: 55px;
          margin: 1px;
          padding: 7px;
          border: 3px solid black;
      }

      #content-box {
          box-sizing: content-box;
          width: 55px;
          height: 55px;
          margin: 1px;
          padding: 7px;
          border: 3px solid black;
      }
      </style>
      <div id="content-box">content-box</div>
      <div id="border-box">border-box</div>
      <div id="output-content">zzz</div>
      <div id="output-border">zzz</div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function dumpDimensions()
      {
          var element;

          element = document.getElementById("content-box");
          document.getElementById("output-content").textContent = "content-box rendered dimensions: " + element.offsetWidth + " x " + element.offsetHeight;
          element = document.getElementById("border-box");
          document.getElementById("output-border").textContent = "border-box rendered dimensions: " + element.offsetWidth + " x " + element.offsetHeight;
      }
  `);

  var contentWidthElement;
  var contentHeightElement;

  function getChildTextByClassName(element, className) {
    var children = element.children;
    for (var i = 0; i < children.length; ++i) {
      if (children[i].classList && children[i].classList.contains(className))
        return children[i].textContent;
    }
    return null;
  }

  function dumpMetrics(sectionElement) {
    var marginElement = sectionElement.getElementsByClassName('margin')[0];
    var borderElement = sectionElement.getElementsByClassName('border')[0];
    var paddingElement = sectionElement.getElementsByClassName('padding')[0];
    var contentDimensions = sectionElement.getElementsByClassName('content')[0].getElementsByTagName('span');
    TestRunner.addResult(
        'margin: ' + getChildTextByClassName(marginElement, 'top') + ' ' +
        getChildTextByClassName(marginElement, 'right') + ' ' + getChildTextByClassName(marginElement, 'bottom') + ' ' +
        getChildTextByClassName(marginElement, 'left'));
    TestRunner.addResult(
        'border: ' + getChildTextByClassName(borderElement, 'top') + ' ' +
        getChildTextByClassName(borderElement, 'right') + ' ' + getChildTextByClassName(borderElement, 'bottom') + ' ' +
        getChildTextByClassName(borderElement, 'left'));
    TestRunner.addResult(
        'padding: ' + getChildTextByClassName(paddingElement, 'top') + ' ' +
        getChildTextByClassName(paddingElement, 'right') + ' ' + getChildTextByClassName(paddingElement, 'bottom') +
        ' ' + getChildTextByClassName(paddingElement, 'left'));
    TestRunner.addResult('content: ' + contentDimensions[0].textContent + ' x ' + contentDimensions[2].textContent);
  }

  function createDoubleClickEvent() {
    var event = document.createEvent('MouseEvent');
    event.initMouseEvent('dblclick', true, true, null, 2, 0, 0, 0, 0, false, false, false, false, 0, null);
    return event;
  }

  var section = Elements.ElementsPanel.ElementsPanel.instance().metricsWidget;

  TestRunner.runTestSuite([
    function testBorderBoxInit1(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('border-box', next);
    },

    async function testInitialBorderBoxMetrics(next) {
      await section.doUpdate();
      var spanElements = section.contentElement.getElementsByClassName('content')[0].getElementsByTagName('span');
      contentWidthElement = spanElements[0];
      contentHeightElement = spanElements[1];
      TestRunner.addResult('=== Initial border-box ===');
      dumpMetrics(section.contentElement);
      contentWidthElement.dispatchEvent(createDoubleClickEvent());
      contentWidthElement.textContent = '60';
      contentWidthElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
      TestRunner.deprecatedRunAfterPendingDispatches(next);
    },

    function testModifiedBorderBoxMetrics(next) {
      TestRunner.addResult('=== Modified border-box ===');
      dumpMetrics(section.contentElement);
      next();
    },

    function testContentBoxInit1(next) {
      ElementsTestRunner.selectNodeWithId('content-box', next);
    },

    async function testInitialContentBoxMetrics(next) {
      await section.doUpdate();
      var spanElements = section.contentElement.getElementsByClassName('content')[0].getElementsByTagName('span');
      contentWidthElement = spanElements[0];
      contentHeightElement = spanElements[1];
      TestRunner.addResult('=== Initial content-box ===');
      dumpMetrics(section.contentElement);
      contentWidthElement.dispatchEvent(createDoubleClickEvent());
      contentWidthElement.textContent = '60';
      contentWidthElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
      TestRunner.deprecatedRunAfterPendingDispatches(next);
      next();
    },

    function testModifiedContentBoxMetrics(next) {
      function callback() {
        next();
      }

      TestRunner.addResult('=== Modified content-box ===');
      dumpMetrics(section.contentElement);
      TestRunner.evaluateInPage('dumpDimensions()', callback);
    }
  ]);
})();
