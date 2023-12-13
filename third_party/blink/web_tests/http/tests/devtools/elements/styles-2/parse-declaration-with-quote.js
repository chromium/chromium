// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that CSSParser correctly parses declarations with unterminated strings. Blink bug 231127\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected" style="color: red ' foo"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', dumpStyles);

  async function dumpStyles() {
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
