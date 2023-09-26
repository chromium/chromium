// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`This test verifies the position and size of the highlighted lines and items in a multiline flex container.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <style>
    #flex-container {
      position: absolute;
      top: 100px;
      left: 100px;
      width: 400px;
      height: 400px;
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
      align-content: space-between;
      justify-content: space-between;
    }
    .item {
      width: 100px;
      height: 100px;
    }
    </style>
    <div id="flex-container">
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
    </div>
    `);

  function dumFlexHighlight(id) {
    return new Promise(resolve => ElementsTestRunner.dumpInspectorHighlightJSON(id, resolve));
  }

  await dumFlexHighlight('flex-container');

  TestRunner.completeTest();
})();
