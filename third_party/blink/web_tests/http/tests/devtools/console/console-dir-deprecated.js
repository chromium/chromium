// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console does not log deprecated warning messages while dir-dumping objects.\n`);

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.loadHTML(`
    <div id="foo"></div>
  `);
  await TestRunner.evaluateInPagePromise(`
     function logObjects()
    {
        console.dir(window);
        console.dir((document.createRange()).expand('document'));
    }
  `);

  TestRunner.evaluateInPage('logObjects()', step2);

  async function step2() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
