// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that source data are extracted correctly from external stylesheets in UTF-8 with BOM. Bug 59322.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <h1 id="inspected">
      I'm red.
      </h1>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/parse-utf8-bom-main.css');

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  async function step1() {
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    TestRunner.completeTest();
  }
})();
