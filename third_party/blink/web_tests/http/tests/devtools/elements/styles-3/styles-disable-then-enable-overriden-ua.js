// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that disabling shorthand removes the "overriden" mark from the UA shorthand it overrides.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <body id="body-id" style="margin: 10px">
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('body-id', step1);

  async function step1() {
    TestRunner.addResult('Before disable');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    ElementsTestRunner.toggleStyleProperty('margin', false);
    ElementsTestRunner.waitForStyles('body-id', step2);
  }

  async function step2() {
    TestRunner.addResult('After disable');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    ElementsTestRunner.toggleStyleProperty('margin', true);
    ElementsTestRunner.waitForStyles('body-id', step3);
  }

  async function step3() {
    TestRunner.addResult('After enable');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    TestRunner.completeTest();
  }
})();
