// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that inspect object action works for function and resolve bound function location.\n`);
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadLegacyModule('sources');
  await TestRunner.showPanel('sources');

  var revealed = 0;
  TestRunner.addSniffer(Sources.SourcesView.prototype, 'showSourceLocation', didReveal, true);
  ConsoleTestRunner.evaluateInConsole('function foo() { }; inspect(foo.bind());inspect(foo);');

  function didReveal(uiSourceCode, lineNumber, columnNumber) {
    TestRunner.addResult('Function was revealed:');
    TestRunner.addResult('lineNumber: ' + lineNumber);
    TestRunner.addResult('columnNumber: ' + columnNumber);
    if (revealed == 0)
      ++revealed;
    else
      TestRunner.completeTest();
  }
})();
