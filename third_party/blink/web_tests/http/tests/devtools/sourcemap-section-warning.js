// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that sourcemap emits warning if there's a section with "url" field.`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  const url = 'http://127.0.0.1:8000/devtools/resources/source-map-warning.html';
  await TestRunner.navigatePromise(url);
  await ConsoleTestRunner.waitForConsoleMessagesPromise(2);
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
