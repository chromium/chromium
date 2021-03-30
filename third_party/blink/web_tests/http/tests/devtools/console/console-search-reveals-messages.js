// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console viewport reveals messages on searching.\n`);

  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    for (var i = 0; i < 200; ++i)
      console.log("Message #" + i);
    console.log("LAST MESSAGE");
  `);

  var consoleView = Console.ConsoleView.instance();
  var viewport = consoleView._viewport;
  const maximumViewportMessagesCount = 150;
  TestRunner.runTestSuite([
    function waitForMessages(next) {
      // NOTE: keep in sync with populateConsoleWithMessages above.
      const expectedMessageCount = 201;
      ConsoleTestRunner.waitForConsoleMessages(expectedMessageCount, next);
    },

    function verifyViewportIsTallEnough(next) {
      viewport.invalidate();
      var viewportMessagesCount = viewport._lastVisibleIndex - viewport._firstVisibleIndex;
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
      TestRunner.addSniffer(consoleView, '_searchFinishedForTests', callback);
      consoleView._searchableView._searchInputElement.value = 'LAST MESSAGE';
      consoleView._searchableView.showSearchField();

      function callback() {
        consoleView._searchableView.handleFindNextShortcut();
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
