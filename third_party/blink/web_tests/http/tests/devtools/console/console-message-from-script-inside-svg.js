// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
    `Tests that message from script inside svg has correct source location.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.navigatePromise('resources/svg.html');
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
