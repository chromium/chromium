// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the coverage list view after finishing recording in the Coverage view.\n`);
  await TestRunner.loadModule('coverage_test_runner');

  await CoverageTestRunner.startCoverage(true);
  await TestRunner.loadHTML(`
      <iframe src="resources/subframe.html"></iframe>
      <p class="class">
      </p>
    `);
  await TestRunner.addStylesheetTag('resources/highlight-in-source.css');
  await TestRunner.addScriptTag('resources/coverage.js');
  await TestRunner.evaluateInPagePromise('performActions(); frames[0].performActionsInFrame()');
  await CoverageTestRunner.stopCoverage();

  const coverageView = Coverage.CoverageView.instance();
  const dataGrid = coverageView._listView._dataGrid;
  for (const child of dataGrid.rootNode().children) {
    const data = child._coverageInfo;
    const url = TestRunner.formatters.formatAsURL(data.url());
    if (url.startsWith('test://'))
      continue;
    const type = Coverage.coverageTypeToString(data.type());
    TestRunner.addResult(`${url} ${type} used: ${data.usedSize()} unused: ${data.unusedSize()} total: ${data.size()}`);
  }

  await CoverageTestRunner.dumpDecorations('highlight-in-source.css');
  await CoverageTestRunner.dumpDecorations('coverage.js');

  TestRunner.completeTest();
})();
