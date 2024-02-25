// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Verify that sourcemap emits warning if there's a section with "url" field.`);
  await TestRunner.showPanel('console');
  const url = 'http://127.0.0.1:8000/devtools/resources/source-map-warning.html';
  await TestRunner.navigatePromise(url);
  await ConsoleTestRunner.waitForConsoleMessagesPromise(1);
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
