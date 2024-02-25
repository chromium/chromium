// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Checks that inherited styles from the same source are not duplicated.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      div {
          color: red;
      }
      </style>
      <div>
        <div>
          <div>
            <div id="inspect">Text.</div>
          </div>
        </div>
      </div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspect', step2);

  async function step2() {
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, false);
    TestRunner.completeTest();
  }
})();
