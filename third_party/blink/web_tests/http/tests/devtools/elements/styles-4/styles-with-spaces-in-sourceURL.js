// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Verifies that links for URLs with spaces displayed properly for matched styles.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected"></div>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/style-with-spaces-in-sourceURL.css');

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', onNodeSelected);

  async function onNodeSelected() {
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
