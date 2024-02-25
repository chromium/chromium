// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that distance data produced for overlays are correct.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      body {
        margin: 0;
      }
      .inspected {
        margin: 5px;
        border: solid 10px aqua;
        padding: 15px;
        width: 200px;
        height: 200px;
        background-color: blue;
      }
      .right {
        position: absolute;
        top: 5px;
        left: 300px;
        width: 10px;
        height: 10px;
        background: coral;
      }
      .bottom {
        position: absolute;
        top: 300px;
        left: 5px;
        width: 10px;
        height: 10px;
        background: coral;
      }

      </style>
      <div id="inspected" class="inspected"></div>
      <div class="right"></div>
      <div class="bottom"></div>
    `);

  ElementsTestRunner.dumpInspectorDistanceJSON('inspected', () => {
    TestRunner.completeTest();
  });
})();
