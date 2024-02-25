// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`This test verifies the gap information sent to the overlay frontend for flex contains with gaps.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <style>
      .container {
        position: absolute;
        top: 100px;
        left: 100px;
        width: 400px;
        height: 400px;
        display: flex;
        flex-wrap: wrap;
        flex-direction: row;
        place-content: flex-start;
        column-gap: 10px;
        row-gap: 20px;
      }
      .item {
        width: 100px;
        height: 100px;
      }
    </style>
    <div class="container" id="test-1" style="flex-direction:row;">
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
    </div>
    <div class="container" id="test-2" style="flex-direction:row-reverse;">
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
    </div>
    <div class="container" id="test-3" style="flex-direction:column;">
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
    </div>
    <div class="container" id="test-4" style="flex-direction:column-reverse;">
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

  await dumFlexHighlight('test-1');
  await dumFlexHighlight('test-2');
  await dumFlexHighlight('test-3');
  await dumFlexHighlight('test-4');

  TestRunner.completeTest();
})();
