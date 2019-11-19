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

  var coverageView = self.runtime.sharedInstance(Coverage.CoverageView);
  var dataGrid = coverageView._listView._dataGrid;
  for (var child of dataGrid.rootNode().children) {
    var data = child._coverageInfo;
    var url = TestRunner.formatters.formatAsURL(data.url());
    if (url.startsWith('test://'))
      continue;
    var type = Coverage.CoverageListView._typeToString(data.type());
    TestRunner.addResult(`${url} ${type} used: ${data.usedSize()} unused: ${data.unusedSize()} total: ${data.size()}`);
  }

  await CoverageTestRunner.dumpDecorations('highlight-in-source.css');
  await CoverageTestRunner.dumpDecorations('coverage.js');

  TestRunner.completeTest();
})();
