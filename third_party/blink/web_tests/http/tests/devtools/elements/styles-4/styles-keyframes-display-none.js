// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that keyframes are shown in styles pane inside display:none.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <style>
      @keyframes animName {
        from { color: green; }
        to { color: lime; }
      }
      #container {
        animation: animName 1000s;
        display: none;
      }
      #element {
        animation: inherit;
      }
    </style>
    <div id="container">
      <div id="element"></div>
    </div>
  `);

  ElementsTestRunner.selectNodeAndWaitForStyles('element', step1);

  async function step1() {
    TestRunner.addResult('=== #element styles ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    ElementsTestRunner.selectNodeAndWaitForStyles('container', step2);
  }

  async function step2() {
    TestRunner.addResult('=== #container styles ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
