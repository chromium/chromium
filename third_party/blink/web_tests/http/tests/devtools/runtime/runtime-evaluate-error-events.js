// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that Runtime.evaluate triggers error handlers\n`);
  await TestRunner.loadLegacyModule('console');
  await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.evaluateInPagePromise(`
      window.onerror = (...args) => console.log('onerror handler:', ...args);
      window.addEventListener('error', event => console.log('error event listener:', event));
  `);

  await TestRunner.RuntimeAgent.evaluate('throw new Error()');
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
