// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`This test verifies the position and size of the highlight rectangles overlayed on an inspected CSS masonry div.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <style>
      body {
          width: 2000px;
          height: 2000px;
          background-color: grey;
      }
      .masonry-columns {
          display: masonry;
          grid-template-columns: 50px 20% 30%;
          width: 500px;
          gap: 10px;
      }
      .masonry-rows {
          display: masonry;
          grid-template-rows: 100px repeat(2, 30px 10%);
          masonry-direction: row;
          height: 500px;
          width: 300px;
          gap: 20px;
      }
      .empty-masonry {
          display: masonry;
          width: 200px;
          height: 200px;
          grid-template-columns: 10% 20%;
      }
    </style>
    <div id="masonry-columns-container" class="masonry-columns">
        <div style="width: 100%; height: 50px;"></div>
        <div style="width: 100%; height: 30px;"></div>
        <div style="width: 100%; height: 25px;"></div>
        <div style="width: 100%; height: 100px;"></div>
        <div style="width: 100%; height: 80px;"></div>
        <div style="width: 100%; height: 60px;"></div>
    </div>
    <div id="masonry-rows-container" class="masonry-rows">
        <div style="width:50px; height: 100%;"></div>
        <div style="width:100px; height: 100%;"></div>
        <div style="width:150px; height: 100%;"></div>
        <div style="width:200px; height: 100%;"></div>
        <div style="width:250px; height: 100%;"></div>
        <div style="width:30px; height: 100%;"></div>
    </div>
    <div id="empty-masonry-container" class="empty-masonry"></div>
    <p id="description">This test verifies the position and size of the highlight rectangles overlayed on an inspected CSS masonry div.</p>
    `);

  function dumpMasonryHighlight(id) {
    return new Promise(resolve => ElementsTestRunner.dumpInspectorHighlightJSON(id, resolve));
  }

  await dumpMasonryHighlight('masonry-columns-container');
  await dumpMasonryHighlight('masonry-rows-container');
  await dumpMasonryHighlight('empty-masonry-container');

  TestRunner.completeTest();
})();
