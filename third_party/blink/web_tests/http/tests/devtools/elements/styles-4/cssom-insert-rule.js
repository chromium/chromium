// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that edited stylesheets appear properly.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
        .rule1 {
          color: green;
        }
        .rule3 {
          color: grey;
        }
      </style>
      <div class="rule0 rule1 rule2 rule3 rule4" id="inspected">Text</div>
    `);

  await TestRunner.evaluateInPagePromise(`
      const sheet = document.querySelector('style').sheet;
      sheet.insertRule('.rule0 {color: black}', 0);
      sheet.insertRule('.rule2 {color: yellow}', 2);
      sheet.insertRule('.rule4 {color: white}', 4);
      sheet.deleteRule(3);
  `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', dump);

  async function dump() {
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
