// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Test that relative links and links with hash open in the sources panel.\n');

  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.addScriptTag('../resources/source3.js');
  await TestRunner.evaluateInPagePromise('foo()');
  var messages = Console.ConsoleView.instance()._visibleViewMessages;

  TestRunner.runTestSuite([
    async function testClickRelativeLink(next) {
      // Ordering is important here, as accessing the element the first time around
      // triggers live location creation and updates which we need to await properly.
      const element = messages[0].element();
      await TestRunner.waitForPendingLiveLocationUpdates();
      const clickTarget = element.querySelectorAll('.console-message-text .devtools-link')[1];
      TestRunner.addResult('Clicking link ' + clickTarget.textContent);
      UI.inspectorView._tabbedPane.once(UI.TabbedPane.Events.TabSelected).then(() => {
        TestRunner.addResult('Panel ' + UI.inspectorView._tabbedPane._currentTab.id + ' was opened.');
        next();
      });
      clickTarget.click();
    },

    function testClickURLWithHash(next) {
      UI.inspectorView._tabbedPane.once(UI.TabbedPane.Events.TabSelected).then(() => {
        TestRunner.addResult('Panel ' + UI.inspectorView._tabbedPane._currentTab.id + ' was opened.');
        var clickTarget = messages[1].element().querySelectorAll('.console-message-text .devtools-link')[0];
        TestRunner.addResult('Clicking link ' + clickTarget.textContent);
        UI.inspectorView._tabbedPane.once(UI.TabbedPane.Events.TabSelected).then(() => {
          TestRunner.addResult('Panel ' + UI.inspectorView._tabbedPane._currentTab.id + ' was opened.');
          next();
        });
        clickTarget.click();
      });
      TestRunner.showPanel('console');
    }
  ]);

  InspectorFrontendHost.openInNewTab = function() {
    TestRunner.addResult('Failure: Open link in new tab!!');
    TestRunner.completeTest();
  };
})();
