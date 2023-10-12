// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests resource tree model on page reload, compares resource tree against golden. Every line is important.\n`);
  await TestRunner.showPanel('resources');
  await TestRunner.navigatePromise(TestRunner.url('resources/resource-tree-reload.html'));
  await TestRunner.reloadPagePromise();
  await Promise.all([
    TestRunner.waitForUISourceCode('styles-initial.css'),
    TestRunner.waitForUISourceCode('script-initial.js'),
    TestRunner.waitForUISourceCode('styles-initial-2.css'),
    TestRunner.waitForUISourceCode('resource-tree-reload-iframe.html'),
  ]);
  ApplicationTestRunner.dumpResourceTreeEverything();
  TestRunner.completeTest();
})();
