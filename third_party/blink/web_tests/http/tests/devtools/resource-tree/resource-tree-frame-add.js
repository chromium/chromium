// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests resource tree model on iframe addition, compares resource tree against golden. Every line is important.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('resources');
  await TestRunner.addStylesheetTag('resources/styles-initial.css');

  TestRunner.addResult('Before addition');
  TestRunner.addResult('====================================');
  ApplicationTestRunner.dumpResourceTreeEverything();
  ConsoleTestRunner.addConsoleSniffer(step2);
  TestRunner.evaluateInPageAnonymously(`
    (function createIframe() {
      var iframe = document.createElement("iframe");
      iframe.src = "${TestRunner.url('resources/resource-tree-frame-add-iframe.html')}";
      document.body.appendChild(iframe);
    })();
  `);

  function step2() {
    TestRunner.addResult('');
    TestRunner.addResult('After addition');
    TestRunner.addResult('====================================');
    ApplicationTestRunner.dumpResourceTreeEverything();
    TestRunner.completeTest();
  }
})();
