// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console viewport reveals messages on searching.\n`);

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    for (var i = 0; i < 200; ++i)
      console.log("Message #" + i);
    console.log("LAST MESSAGE");
  `);

  var consoleView = Console.ConsoleView.instance();
  var viewport = consoleView.viewport;
  const maximumViewportMessagesCount = 150;
  TestRunner.runTestSuite([
    function waitForMessages(next) {
      // NOTE: keep in sync with populateConsoleWithMessages above.
      const expectedMessageCount = 201;
      ConsoleTestRunner.waitForConsoleMessages(expectedMessageCount, next);
    },

    function verifyViewportIsTallEnough(next) {
      viewport.invalidate();
      var viewportMessagesCount = viewport.lastVisibleIndex - viewport.firstVisibleIndex;
      if (viewportMessagesCount > maximumViewportMessagesCount) {
        TestRunner.addResult(
          String.sprintf(
            'Test cannot be run because viewport could fit %d messages which is more than maximum of %d.',
            viewportMessagesCount,
            maximumViewportMessagesCount
          )
        );
        TestRunner.completeTest();
        return;
      }
      next();
    },

    function scrollConsoleToTop(next) {
      viewport.forceScrollItemToBeFirst(0);
      dumpTop();
      next();
    },

    function testFindLastMessage(next) {
      TestRunner.addSniffer(consoleView, 'searchFinishedForTests', callback);
      consoleView.searchableView().searchInputElement.value = 'LAST MESSAGE';
      consoleView.searchableView().showSearchField();

      function callback() {
        consoleView.searchableView().handleFindNextShortcut();
        dumpBottom();
        next();
      }
    }
  ]);

  function dumpTop() {
    viewport.refresh();
    TestRunner.addResult('first visible message index: ' + viewport.firstVisibleIndex());
  }

  function dumpBottom() {
    viewport.refresh();
    TestRunner.addResult('last visible message index: ' + viewport.lastVisibleIndex());
  }
})();
