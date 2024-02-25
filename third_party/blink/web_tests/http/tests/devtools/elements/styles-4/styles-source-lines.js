// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that proper source lines are reported for the parsed styles.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.navigatePromise('resources/styles-source-lines-helper.html');

  ElementsTestRunner.selectNodeAndWaitForStyles('main', step1);

  async function step1() {
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    TestRunner.completeTest();
  }
})();
