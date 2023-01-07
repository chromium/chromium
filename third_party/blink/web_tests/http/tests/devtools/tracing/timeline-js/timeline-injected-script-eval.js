// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline API function call is not recorded for InjectedScript.eval.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
      }
  `);

  UI.panels.timeline.disableCaptureJSProfileSetting.set(true);
  await PerformanceTestRunner.performActionsAndPrint('performActions()', 'FunctionCall');
})();
