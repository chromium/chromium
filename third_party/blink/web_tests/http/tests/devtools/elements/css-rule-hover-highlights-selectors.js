// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      .border {
          border: 1px solid black;
      }
      </style>
      <div class="border">1st</div>
      <div id="initial"></div>
      <div id="inspected" class="border">2nd</div>
      <div class="border">3rd</div>
      <template id="dom-template">
          <style>
          .bck {
              border: 1px solid black;
          }
          </style>
          <div class="bck">1st</div>
          <div class="bck">2nd</div>
          <div class="bck">3rd</div>
          <div class="bck">4th</div>
          <div class="bck" id="fifth">5th</div>
      </template>
    `);
  await TestRunner.evaluateInPagePromise(`
      function requestAnimationFramePromise()
      {
          return new Promise(fulfill => requestAnimationFrame(fulfill));
      }

      var host = document.querySelector("body");
      var root = host.attachShadow({mode: 'open'});
      var template = document.querySelector("#dom-template");
      var clone = document.importNode(template.content, true);
      root.appendChild(clone);
      var second = root.querySelector("#fifth");
      second.id = "inspected-shadow";
  `);

  TestRunner.runTestSuite([
    function setupProxyOverlay(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('initial', onSelected);

      function onSelected() {
        var section = ElementsTestRunner.firstMatchedStyleSection();
        section.highlight();
        TestRunner.callFunctionInPageAsync('requestAnimationFramePromise').then(onHighlighted);
      }

      function onHighlighted() {
        TestRunner.evaluateFunctionInOverlay(drawHighlightProxy, next);
      }
    },

    function testRegularNodeSelection(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', onSelected);

      function onSelected() {
        resetHighlightCount(onHighlightCountReset);
      }

      function onHighlightCountReset() {
        var section = ElementsTestRunner.firstMatchedStyleSection();
        section.highlight();
        TestRunner.callFunctionInPageAsync('requestAnimationFramePromise').then(onHighlighted);
      }

      function onHighlighted() {
        dumpHighlightCount(next);
      }
    },

    function testShadowDOMNodeSelection(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected-shadow', onSelected);

      function onSelected() {
        resetHighlightCount(onHighlightCountReset);
      }

      function onHighlightCountReset() {
        var section = ElementsTestRunner.firstMatchedStyleSection();
        section.highlight();
        TestRunner.callFunctionInPageAsync('requestAnimationFramePromise').then(onHighlighted);
      }

      function onHighlighted() {
        dumpHighlightCount(next);
      }
    },
  ]);

  function drawHighlightProxy() {
    window._highlightsForTest = [];
    var oldDispatch = dispatch;
    dispatch = proxy;

    function proxy(message) {
      const functionName = message[0];
      if (functionName === 'drawHighlight') {
        window._highlightsForTest.push(message[1]);
      }
      oldDispatch(message);
    }
  }

  function reportHighlights() {
    var result = window._highlightsForTest.length;
    window._highlightsForTest = [];
    return result + '';
  }

  function dumpHighlightCount(next) {
    TestRunner.evaluateFunctionInOverlay(reportHighlights, onResults);

    function onResults(count) {
      TestRunner.addResult('Highlights drawn: ' + count);
      next();
    }
  }

  function resetHighlightCount(next) {
    TestRunner.evaluateFunctionInOverlay(reportHighlights, next);
  }
})();
