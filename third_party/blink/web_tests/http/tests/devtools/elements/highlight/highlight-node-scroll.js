// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `This test verifies the position and size of the highlight rectangles overlayed on an inspected div in the scrolled view.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>

      body {
          width: 2000px;
          height: 2000px;
          background-color: grey;
      }
      .inspected {
          margin: 5px;
          border: solid 10px aqua;
          padding: 15px;
          width: 200px;
          height: 200px;
          background-color: blue;
          float: left;
      }
      #scrollingContainer {
          clear: both;
          width: 100px;
          height: 100px;
          overflow: auto;
      }
      #description {
          clear: both;
      }

      </style>
      <div id="inspectedElement1" class="inspected"></div>

      <div id="scrollingContainer">
          <div id="inspectedElement2" class="inspected"></div>
      </div>

      <p id="description"></p>
    `);
  await TestRunner.evaluateInPagePromise(`
      window.scrollBy(50, 100);
      scrollingContainer = document.getElementById("scrollingContainer");
      scrollingContainer.scrollTop = 50;
      scrollingContainer.scrollLeft = 60;
  `);

  ElementsTestRunner.dumpInspectorHighlightJSON('inspectedElement1', testNode2);

  function testNode2() {
    ElementsTestRunner.dumpInspectorHighlightJSON('inspectedElement2', TestRunner.completeTest.bind(TestRunner));
  }
})();
