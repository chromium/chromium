// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`This test verifies that similarly-sized grids with different writing-modes share the same grid information but have a different writingMode value.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      .grid {
        position: absolute;
        top: 50px;
        left: 50px;
        display: grid;
        grid-template-rows: 20px 50px;
        grid-template-columns: 100px 200px;
        gap: 10px;
      }
      #verticalRl {
        writing-mode: vertical-rl;
      }
      #verticalLr {
        writing-mode: vertical-lr;
      }
      #sidewaysRl {
        writing-mode: sideways-rl;
      }
      #sidewaysLr {
        writing-mode: sideways-lr;
      }
      </style>

      <p id="description">This test verifies that similarly-sized grids with different writing-modes share the same grid information but have a different writingMode value.</p>
      <div>
          <div class="grid" id="horizontalTb">
              <div style="background: burlywood">1</div>
              <div style="background: cadetblue">2</div>
              <div style="background: aquamarine">3</div>
              <div style="background: peachpuff">4</div>
          </div>
          <div class="grid" id="verticalRl">
            <div style="background: burlywood">1</div>
            <div style="background: cadetblue">2</div>
            <div style="background: aquamarine">3</div>
            <div style="background: peachpuff">4</div>
          </div>
          <div class="grid" id="verticalLr">
            <div style="background: burlywood">1</div>
            <div style="background: cadetblue">2</div>
            <div style="background: aquamarine">3</div>
            <div style="background: peachpuff">4</div>
          </div>
          <div class="grid" id="sidewaysRl">
            <div style="background: burlywood">1</div>
            <div style="background: cadetblue">2</div>
            <div style="background: aquamarine">3</div>
            <div style="background: peachpuff">4</div>
          </div>
          <div class="grid" id="sidewaysLr">
            <div style="background: burlywood">1</div>
            <div style="background: cadetblue">2</div>
            <div style="background: aquamarine">3</div>
            <div style="background: peachpuff">4</div>
          </div>
      </div>
  `);

  function getWritingMode(highlightObject) {
    return highlightObject.gridInfo[0].writingMode;
  }

  function getGridInfo(highlightObject) {
    const info = highlightObject.gridInfo[0];
    // Drop the writingMode property as it's the only one that differs, and we want to compare the rest.
    return JSON.stringify(info, (key, value) => key === 'writingMode' ? undefined : value, 2)
  }

  const horizontalTbNode = await ElementsTestRunner.nodeWithIdPromise('horizontalTb');
  const horizontalTbHighlight = await TestRunner.OverlayAgent.getHighlightObjectForTest(horizontalTbNode.id);
  const horizontalTbInfo = getGridInfo(horizontalTbHighlight);

  TestRunner.addResult(`Node id #horizontalTb writing-mode: ${getWritingMode(horizontalTbHighlight)}`);
  TestRunner.addResult(`Grid info: ${horizontalTbInfo}`);

  for (const id of ['verticalRl', 'verticalLr', 'sidewaysRl', 'sidewaysLr']) {
    const node = await ElementsTestRunner.nodeWithIdPromise(id);
    const result = await TestRunner.OverlayAgent.getHighlightObjectForTest(node.id);
    const gridInfo = getGridInfo(result);

    TestRunner.addResult(`Node id #${id} writing-mode: ${getWritingMode(result)}`);
    TestRunner.addResult(`Grid info: ${gridInfo}`);
    TestRunner.addResult(`Should be the same as #horizontalTb: ${gridInfo === horizontalTbInfo}`)
  }

  TestRunner.completeTest();
})();
