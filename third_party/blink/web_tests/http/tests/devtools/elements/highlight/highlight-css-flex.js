// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`This test verifies the position and size of the highlight rectangles overlayed on an inspected CSS flex div.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #flex-container {
        width: 500px;
        height: 100px;
        display: flex;
      }
      .item {
        margin: 10px;
        flex: 1;
      }
      button {
        width: 50px;
        height: 50px;
        border: 0;
        padding: 0;
      }
      </style>
      <div id="flex-container">
        <div class="item"></div>
        <div class="item"></div>
        <div class="item"></div>
      </div>
      <button id="should-not-be-flexbox">click</button>
      <p id="description">This test verifies the position and size of the highlight rectangles overlayed on an inspected CSS flex div.</p>
    `);

  function dumFlexHighlight(id) {
    return new Promise(resolve => ElementsTestRunner.dumpInspectorHighlightJSON(id, resolve));
  }

  await dumFlexHighlight('flex-container');
  await dumFlexHighlight('should-not-be-flexbox');

  TestRunner.completeTest();
})();
