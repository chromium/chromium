// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that style updates are throttled during DOM traversal. Bug 77643.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div></div>
      <div></div>
      <div></div>
      <div></div>
      <div id="inspected"></div>
    `);

  var updateCount = 0;
  var keydownCount = 5;

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', selectCallback);
  function selectCallback() {
    TestRunner.addSniffer(Elements.StylesSidebarPane.prototype, 'innerRebuildUpdate', sniffUpdate, true);
    var element = ElementsTestRunner.firstElementsTreeOutline().element;
    for (var i = 0; i < keydownCount; ++i)
      element.dispatchEvent(TestRunner.createKeyEvent('ArrowUp'));

    TestRunner.deprecatedRunAfterPendingDispatches(completeCallback);
  }

  function completeCallback() {
    if (updateCount >= keydownCount)
      TestRunner.addResult('ERROR: got ' + updateCount + ' updates for ' + keydownCount + ' consecutive keydowns');
    else
      TestRunner.addResult('OK: updates throttled');
    TestRunner.completeTest();
  }

  function sniffUpdate() {
    ++updateCount;
  }
})();
