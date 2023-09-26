// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`This test verifies that huge CSS grids can be highlighted with the grid Highlight Tool.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #big-grid {
          display: grid;
          grid-template-columns: repeat(100, 5px);
          grid-template-rows: repeat(100, 5px);
          grid-gap: 5px;
      }
      #huge-grid {
          display: grid;
          grid-template-columns: repeat(10000, 5px);
          grid-template-rows: repeat(10000, 5px);
          grid-gap: 5px;
      }
      </style>
      <div id="big-grid"></div>
      <div id="huge-grid"></div>

      <p id="description">This test verifies that huge CSS grids can be highlighted.</p>
    `);

  // Dump the highlight of a 100x100 grid to check that it's not absurdly huge.
  await new Promise(resolve => {
    ElementsTestRunner.dumpInspectorGridHighlightsJSON(['big-grid'], resolve);
  });


  // Now check that the highlight data for a 10000x10000 grid can be generated.
  // But don't dump it since that would make the test time out.
  const node = await ElementsTestRunner.nodeWithIdPromise('huge-grid');
  await TestRunner.OverlayAgent.getGridHighlightObjectsForTest([node.id]);

  TestRunner.completeTest();
})();
