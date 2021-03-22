// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the CSS highlight in sources after the Pretty print formatting.\n`);
  await TestRunner.loadModule('coverage_test_runner');
  await TestRunner.loadHTML(`
      <p id="id">PASS</p>
    `);
  await TestRunner.addStylesheetTag('resources/decorations-after-inplace-formatter.css');
  await TestRunner.addStylesheetTag('resources/long-mangled.css');

  await CoverageTestRunner.startCoverage();
  await TestRunner.evaluateInPagePromise('performActions()');
  await CoverageTestRunner.stopCoverage();
  var node = CoverageTestRunner.findCoverageNodeForURL('long-mangled.css');
  var coverageListView = Coverage.CoverageView.instance()._listView;
  var decoratePromise = TestRunner.addSnifferPromise(Coverage.CoverageView.LineDecorator.prototype, '_innerDecorate');
  node.select();
  coverageListView._revealSourceForSelectedNode();
  await decoratePromise;
  TestRunner.addResult('The below should be formatted');
  CoverageTestRunner.dumpDecorationsInSourceFrame(UI.panels.sources.visibleView);

  TestRunner.completeTest();
})();
