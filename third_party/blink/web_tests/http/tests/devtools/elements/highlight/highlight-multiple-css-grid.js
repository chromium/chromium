// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`This test verifies the position and size of the highlight rectangles overlayed on an inspected CSS grid div.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>

      body {
          width: 2000px;
          height: 2000px;
          background-color: grey;
      }
      .outer {
          transform: rotate(-90deg) translate(-200px, -900px);
      }
      .grid {
          width: 1000px;
          display: grid;
          grid-template-columns: 50px 20%;
          grid-auto-rows: 25px;
      }
      .padded {
          padding: 50px 60px;
          align-content: space-around;
          justify-content: end;
          grid-gap: 15px 25px;
          border: 5px solid;
          margin: 10px;
      }
      .standard {
          width: 1300px;
          display: grid;
          grid-template-columns: 50px 1200px;
          grid-auto-rows: 100px;
          grid-gap: 20px 50px;
      }
      .fixed {
          position: absolute;
          top: 0;
          left: 0;
      }
      </style>
      <div class="outer">
        <div id="grid1" class="grid padded fixed">
            <div>c1</div>
            <div>c2</div>
            <div>c3</div>
        </div>
        <div id="grid2" class="standard fixed">
            <div>Cell 1</div>
            <div>Cell 2</div>
            <div>Cell 3</div>
        </div>
      </div>

      <p id="description">This test verifies the position and size of the highlight rectangles overlayed on an multiple CSS grid divs.</p>
    `);

  function dumpGridHighlight(ids) {
    return new Promise(resolve => ElementsTestRunner.dumpInspectorGridHighlightsJSON(ids, resolve));
  }

  await dumpGridHighlight(['grid1', 'grid2']);

  TestRunner.completeTest();
})();
