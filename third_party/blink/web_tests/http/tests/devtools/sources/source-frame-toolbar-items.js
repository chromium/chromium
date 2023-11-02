// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Checks tool bar items for scripts');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await SourcesTestRunner.startDebuggerTestPromise();
  TestRunner.evaluateInPagePromise(`
    function testFunction() {
      eval('foo()//# sourceURL=test.js');

      function foo() {
        eval('debugger;');
      }
    }
    //# sourceURL=foo.js`);

  await SourcesTestRunner.runTestFunctionAndWaitUntilPausedPromise();
  await TestRunner.addSnifferPromise(
            Sources.ScriptOriginPlugin.prototype, 'rightToolbarItems');

  TestRunner.addResult('Items for foo.js:');
  await dumpToolbarItems(Sources.SourcesPanel.instance().visibleView);
  TestRunner.addResult('Items for test.js:');
  await dumpToolbarItems(await SourcesTestRunner.showScriptSourcePromise('test.js'));

  SourcesTestRunner.completeDebuggerTest();

  async function dumpToolbarItems(sourceFrame) {
    const items = await sourceFrame.toolbarItems();
    // Toolbar items have live locations.
    await TestRunner.waitForPendingLiveLocationUpdates();
    for (let item of items) {
      TestRunner.addResult(item.element.deepTextContent());
    }
  }
})();
