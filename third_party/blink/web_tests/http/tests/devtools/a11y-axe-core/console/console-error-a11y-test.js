// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadModule('axe_core_test_runner');
  TestRunner.addResult(
      'Tests accessibility of console containing an error message using the axe-core linter.');

  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  const widget = await UI.viewManager.view('console').widget();

  async function callback() {
    ConsoleTestRunner.dumpConsoleMessages();
    await AxeCoreTestRunner.runValidation(widget.element);
    TestRunner.completeTest();
  }

  ConsoleTestRunner.evaluateInConsole('invalidVar123', callback);
})();
