// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that oopif iframes are rendered inline.\n`);
  await TestRunner.showPanel('elements');

  // Save time on style updates.
  ElementsTestRunner.ignoreSidebarUpdates();

  await TestRunner.navigatePromise('resources/page.html');
  await ElementsTestRunner.expandAndDump();

  TestRunner.completeTest();
})();
