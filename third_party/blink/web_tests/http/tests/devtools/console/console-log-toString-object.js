// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that passing an object which throws on string conversion into console.log won't crash inspected Page. The test passes if it doesn't crash. Bug 57557\n`);
  await TestRunner.showPanel('console');
  await TestRunner.navigatePromise('resources/console-log-toString-object.html');
  await TestRunner.reloadPagePromise();
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
