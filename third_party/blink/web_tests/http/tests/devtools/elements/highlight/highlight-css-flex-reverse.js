// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`This test verifies that the flex overlay creates the right lines and items for reverse direction flex containers. See crbug.com/1153272.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <style>
    .container {
      display: flex;
      justify-content: center;
      width: 100px;
      height: 100px;
      gap: 10px;
      align-items: flex-start;
      background: gold;
    }
    .item { padding: 10px; }
    </style>
    <div class="container" id="test-1" style="flex-direction:row-reverse;">
      <div class="item"></div>
      <div class="item" style="align-self: flex-end;"></div>
      <div class="item"></div>
    </div>
    <div class="container" id="test-2" style="flex-direction:column-reverse;">
      <div class="item"></div>
      <div class="item" style="align-self: flex-end;"></div>
      <div class="item"></div>
    </div>
  `);

  function dumFlexHighlight(id) {
    return new Promise(resolve => ElementsTestRunner.dumpInspectorHighlightJSON(id, resolve));
  }

  await dumFlexHighlight('test-1');
  await dumFlexHighlight('test-2');

  TestRunner.completeTest();
})();
