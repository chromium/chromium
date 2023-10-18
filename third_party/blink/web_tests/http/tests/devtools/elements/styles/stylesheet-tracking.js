// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that the styleSheetAdded and styleSheetRemoved events are reported into the frontend. Bug 105828. https://bugs.webkit.org/show_bug.cgi?id=105828\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.navigatePromise('resources/stylesheet-tracking-main.html');
  await TestRunner.addStylesheetTag('stylesheet-tracking.css');
  await TestRunner.evaluateInPagePromise(`
      function addStyleSheet()
      {
          var styleElement = document.createElement("style");
          styleElement.id = "style";
          styleElement.type = "text/css";
          styleElement.textContent = "@import url(\\"stylesheet-tracking-import.css\\");\\na { color: green; }"
          document.head.appendChild(styleElement);
      }

      function removeImport()
      {
          document.getElementById("style").sheet.deleteRule(0);
      }

      function removeStyleSheet()
      {
          document.head.removeChild(document.getElementById("style"));
      }

      function loadIframe()
      {
          var iframe = document.createElement("iframe");
          iframe.setAttribute("seamless", "seamless");
          iframe.src = "stylesheet-tracking-iframe.html";
          document.body.appendChild(iframe);
      }

      function iframe()
      {
          return document.getElementsByTagName("iframe")[0];
      }

      function addIframeStyleSheets()
      {
          iframe().contentWindow.postMessage("addStyleSheets", "*");
      }

      function navigateIframe()
      {
          iframe().src = iframe().src;
      }

      function removeIframeStyleSheets()
      {
          iframe().contentWindow.postMessage("removeStyleSheets", "*");
      }

      function removeIframe()
      {
          var element = iframe();
          element.parentElement.removeChild(element);
      }
  `);

  var inspectedNode;

  TestRunner.cssModel.addEventListener(SDK.CSSModel.Events.StyleSheetAdded, styleSheetAdded, null);
  TestRunner.cssModel.addEventListener(SDK.CSSModel.Events.StyleSheetRemoved, styleSheetRemoved, null);
  var headers = TestRunner.cssModel.styleSheetHeaders();
  TestRunner.addResult(headers.length + ' headers known:');
  sortAndDumpData(headers);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step0);

  function step0(node) {
    inspectedNode = node;
    TestRunner.addResult('=== Adding iframe... ===');
    TestRunner.evaluateInPage('loadIframe()');
    waitStyleSheetAdded(1, iframeAdded);

    function iframeAdded() {
      TestRunner.addResult('=== Adding iframe stylesheet... ===');
      waitStyleSheetAdded(2, navigateIframe);
      TestRunner.evaluateInPage('addIframeStyleSheets()');
    }

    function navigateIframe() {
      TestRunner.addResult('=== Navigating iframe... ===');
      waitStyleSheetRemoved(3, frameNavigated);
      TestRunner.evaluateInPage('navigateIframe()');

      function frameNavigated() {
        waitStyleSheetAdded(1, styleSheetsAdded);
      }

      function styleSheetsAdded() {
        TestRunner.addResult('=== Adding iframe stylesheet... ===');
        waitStyleSheetAdded(2, removeIframeStyleSheets);
        TestRunner.evaluateInPage('addIframeStyleSheets()');
      }
    }

    function removeIframeStyleSheets() {
      TestRunner.addResult('=== Removing iframe stylesheet... ===');
      waitStyleSheetRemoved(2, step1);
      TestRunner.evaluateInPage('removeIframeStyleSheets()');
    }
  }

  function step1() {
    TestRunner.addResult('=== Adding stylesheet... ===');
    waitStyleSheetAdded(4, step2);
    TestRunner.evaluateInPage('addStyleSheet()');
  }

  function step2() {
    TestRunner.addResult('=== Removing @import... ===');
    waitStyleSheetRemoved(3, step3);
    TestRunner.evaluateInPage('removeImport()');
  }

  function step3() {
    TestRunner.addResult('=== Removing stylesheet... ===');
    waitStyleSheetRemoved(1, step4);
    TestRunner.evaluateInPage('removeStyleSheet()');
  }

  function step4() {
    TestRunner.addResult('=== Adding rule... ===');
    waitStyleSheetAdded(1);
    ElementsTestRunner.addNewRule('#inspected', successCallback);

    function successCallback() {
      TestRunner.addResult('Rule added');
      step5();
    }
  }

  function step5() {
    TestRunner.addResult('=== Removing iframe... ===');
    TestRunner.evaluateInPage('removeIframe()');
    waitStyleSheetRemoved(1, step6);
  }

  function step6() {
    TestRunner.completeTest();
  }

  var addedCallback;
  var addedSheetCount;
  var addedSheets = [];

  function waitStyleSheetAdded(count, next) {
    addedSheetCount = count;
    addedCallback = next;
  }

  function styleSheetAdded(event) {
    var header = event.data;
    addedSheets.push(header);
    --addedSheetCount;
    if (addedSheetCount > 0)
      return;
    else if (addedSheetCount < 0)
      TestRunner.addResult('Unexpected styleSheetAdded()');

    TestRunner.addResult('Stylesheets added:');
    sortAndDumpData(addedSheets);
    addedSheets = [];
    if (addedCallback) {
      var callback = addedCallback;
      addedCallback = null;
      callback();
    }
  }

  var removedCallback;
  var removedSheetCount;
  var removedSheets = [];

  function waitStyleSheetRemoved(count, next) {
    removedSheetCount = count;
    removedCallback = next;
  }

  function styleSheetRemoved(event) {
    var header = event.data;
    removedSheets.push(header);
    --removedSheetCount;
    if (removedSheetCount > 0)
      return;
    else if (removedSheetCount < 0)
      TestRunner.addResult('Unexpected styleSheetRemoved()');

    TestRunner.addResult('Stylesheets removed:');
    sortAndDumpData(removedSheets);
    removedSheets = [];
    if (removedCallback) {
      var callback = removedCallback;
      removedCallback = null;
      callback();
    }
  }

  function sortAndDumpData(sheets) {
    function sorter(a, b) {
      return a.sourceURL.localeCompare(b.sourceURL);
    }
    sheets = sheets.sort(sorter);
    for (var i = 0; i < sheets.length; ++i) {
      var header = sheets[i];
      TestRunner.addResult('    URL=' + trimURL(header.sourceURL));
      TestRunner.addResult('    origin=' + header.origin);
      TestRunner.addResult('    isInline=' + header.isInline);
      TestRunner.addResult('    hasSourceURL=' + header.hasSourceURL);
    }
  }

  function trimURL(url) {
    if (!url)
      return url;
    var lastIndex = url.lastIndexOf('devtools/');
    if (lastIndex < 0)
      return url;
    return '.../' + url.substr(lastIndex);
  }
})();
