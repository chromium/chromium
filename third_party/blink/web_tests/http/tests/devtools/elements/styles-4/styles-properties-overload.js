// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Verifies that inspector figures out overloaded properties correctly.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      div {
          margin: 1px;
          border: 1px solid black;
      }

      #inspect {
          margin-top: 1px;
          margin-left: 1px;
          margin-right: 1px;
          margin-bottom: 1px;
          font: 10px Arial;
      }

      .container {
          font-size: 10px;
          border: 0;
      }

      </style>
      <div class="container">
          <div id="inspect">Text.</div>
      </div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspect', step2);

  async function step2() {
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, false);
    TestRunner.completeTest();
  }
})();
