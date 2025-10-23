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

  const nodeIds = [];
  for (const elementId of ['grid1', 'grid2']) {
    const node = await ElementsTestRunner.nodeWithIdPromise(elementId);
    nodeIds.push(node.id);
  }
  const {highlights: {gridHighlights}} =
      await TestRunner.OverlayAgent.invoke_getGridHighlightObjectsForTest({nodeIds});
  TestRunner.assertEquals(2, gridHighlights.length);
  const grid1HighlightExpected = JSON.stringify({
    'rotationAngle': -90,
    'writingMode': 'horizontal-tb',
    'columnTrackSizes': [
      {'x': 173, 'y': 383, 'computedSize': 50, 'authoredSize': '50px'},
      {'x': 173, 'y': 233, 'computedSize': 200, 'authoredSize': '20%'}
    ],
    'rowTrackSizes': [
      {'x': 185.5, 'y': 408, 'computedSize': 25},
      {'x': 225.5, 'y': 408, 'computedSize': 25}
    ],
    'rows': [
      'M', 173, 408, 'L', 173, 133, 'M', 198, 133, 'L', 198, 408,
      'M', 213, 408, 'L', 213, 133, 'M', 238, 133, 'L', 238, 408
    ],
    'rowGaps':
        ['M', 198, 408, 'L', 198, 133, 'L', 213, 133, 'L', 213, 408, 'Z'],
    'columns': [
      'M', 173, 408, 'L', 238, 408, 'M', 238, 358, 'L', 173, 358,
      'M', 173, 333, 'L', 238, 333, 'M', 238, 133, 'L', 173, 133
    ],
    'columnGaps':
        ['M', 173, 358, 'L', 173, 333, 'L', 238, 333, 'L', 238, 358, 'Z'],
    'positiveRowLineNumberPositions':
        [{'x': 173, 'y': 408}, {'x': 205.5, 'y': 408}, {'x': 238, 'y': 408}],
    'positiveColumnLineNumberPositions':
        [{'x': 173, 'y': 408}, {'x': 173, 'y': 345.5}, {'x': 173, 'y': 133}],
    'negativeRowLineNumberPositions': [{'x': 173, 'y': 133}],
    'negativeColumnLineNumberPositions':
        [{'x': 238, 'y': 408}, {'x': 238, 'y': 345.5}, {'x': 238, 'y': 133}],
    'areaNames': {},
    'rowLineNameOffsets': [],
    'columnLineNameOffsets': [],
    'gridBorder':
        ['M', 173, 408, 'L', 173, 133, 'L', 238, 133, 'L', 238, 408, 'Z'],
    'gridHighlightConfig': {
      'gridBorderDash': false,
      'rowLineDash': true,
      'columnLineDash': true,
      'showGridExtensionLines': true,
      'showPositiveLineNumbers': true,
      'showNegativeLineNumbers': true,
      'showAreaNames': true,
      'showLineNames': true,
      'gridBorderColor': 'rgba(255, 0, 0, 0)',
      'rowLineColor': 'rgba(128, 0, 0, 0)',
      'columnLineColor': 'rgba(128, 0, 0, 0)',
      'rowGapColor': 'rgba(0, 255, 0, 0)',
      'columnGapColor': 'rgba(0, 0, 255, 0)',
      'rowHatchColor': 'rgba(255, 255, 255, 0)',
      'columnHatchColor': 'rgba(128, 128, 128, 0)',
      'areaBorderColor': 'rgba(255, 0, 0, 0)',
      'gridBackgroundColor': 'rgba(255, 0, 0, 0)'
    },
    'isPrimaryGrid': true
  });
  const grid2HighlightExpected = JSON.stringify({
    'rotationAngle': -90,
    'writingMode': 'horizontal-tb',
    'columnTrackSizes': [
      {'x': 108, 'y': 1183, 'computedSize': 50, 'authoredSize': '50px'},
      {'x': 108, 'y': 508, 'computedSize': 1200, 'authoredSize': '1200px'}
    ],
    'rowTrackSizes': [
      {'x': 158, 'y': 1208, 'computedSize': 100},
      {'x': 278, 'y': 1208, 'computedSize': 100}
    ],
    'rows': [
      'M', 108, 1208, 'L', 108, -92, 'M', 208, -92, 'L', 208, 1208,
      'M', 228, 1208, 'L', 228, -92, 'M', 328, -92, 'L', 328, 1208
    ],
    'rowGaps':
        ['M', 208, 1208, 'L', 208, -92, 'L', 228, -92, 'L', 228, 1208, 'Z'],
    'columns': [
      'M', 108, 1208, 'L', 328, 1208, 'M', 328, 1158, 'L', 108, 1158,
      'M', 108, 1108, 'L', 328, 1108, 'M', 328, -92,  'L', 108, -92
    ],
    'columnGaps':
        ['M', 108, 1158, 'L', 108, 1108, 'L', 328, 1108, 'L', 328, 1158, 'Z'],
    'positiveRowLineNumberPositions':
        [{'x': 108, 'y': 1208}, {'x': 218, 'y': 1208}, {'x': 328, 'y': 1208}],
    'positiveColumnLineNumberPositions':
        [{'x': 108, 'y': 1208}, {'x': 108, 'y': 1133}, {'x': 108, 'y': -92}],
    'negativeRowLineNumberPositions': [{'x': 108, 'y': -92}],
    'negativeColumnLineNumberPositions':
        [{'x': 328, 'y': 1208}, {'x': 328, 'y': 1133}, {'x': 328, 'y': -92}],
    'areaNames': {},
    'rowLineNameOffsets': [],
    'columnLineNameOffsets': [],
    'gridBorder':
        ['M', 108, 1208, 'L', 108, -92, 'L', 328, -92, 'L', 328, 1208, 'Z'],
    'gridHighlightConfig': {
      'gridBorderDash': false,
      'rowLineDash': true,
      'columnLineDash': true,
      'showGridExtensionLines': true,
      'showPositiveLineNumbers': true,
      'showNegativeLineNumbers': true,
      'showAreaNames': true,
      'showLineNames': true,
      'gridBorderColor': 'rgba(255, 0, 0, 0)',
      'rowLineColor': 'rgba(128, 0, 0, 0)',
      'columnLineColor': 'rgba(128, 0, 0, 0)',
      'rowGapColor': 'rgba(0, 255, 0, 0)',
      'columnGapColor': 'rgba(0, 0, 255, 0)',
      'rowHatchColor': 'rgba(255, 255, 255, 0)',
      'columnHatchColor': 'rgba(128, 128, 128, 0)',
      'areaBorderColor': 'rgba(255, 0, 0, 0)',
      'gridBackgroundColor': 'rgba(255, 0, 0, 0)'
    },
    'isPrimaryGrid': true
  });
  const firstHighlightResult = JSON.stringify(gridHighlights[0]);
  const secondHighlightResult = JSON.stringify(gridHighlights[1]);
  // The order of the result doesn't matter, as long as they match expected
  // highlights for grid1 and grid2.
  TestRunner.assertTrue(
      (grid1HighlightExpected === firstHighlightResult &&
       grid2HighlightExpected === secondHighlightResult) ||
      (grid1HighlightExpected === secondHighlightResult &&
       grid2HighlightExpected === firstHighlightResult));

  TestRunner.completeTest();
})();
