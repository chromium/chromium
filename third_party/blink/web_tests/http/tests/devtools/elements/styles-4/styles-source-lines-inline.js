// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that elements panel shows proper inline style locations in the sidebar panel.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.navigatePromise('resources/styles-source-lines-inline-helper.html');

  ElementsTestRunner.selectNodeAndWaitForStyles('foo', step2);

  async function step2() {
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    TestRunner.completeTest();
  }
})();
