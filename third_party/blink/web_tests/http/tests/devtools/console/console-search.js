// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Platform from 'devtools/core/platform/platform.js';
import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(`Tests console search.\n`);

  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    console.log("FIRST MATCH, SECOND MATCH");
    for (var i = 0; i < 200; ++i)
      console.log("Message #" + i);
    var a = {};
    for (var i = 0; i < 200; ++i)
      a["field_" + i] = "value #" + i;
    console.dir(a);
    console.log("LAST MATCH");
  `);

  function addResult(result) {
    viewport.refresh();
    TestRunner.addResult(result);
  }

  function setQuery(text, isRegex, caseSensitive, callback) {
    TestRunner.addSniffer(consoleView, 'searchFinishedForTests', callback);
    consoleView.searchableView().searchInputElement.value = text;
    consoleView.searchableView().regexButton.setToggled(isRegex);
    consoleView.searchableView().caseSensitiveButton.setToggled(caseSensitive);
    consoleView.searchableView().showSearchField();
  }

  function matchesText() {
    return consoleView.searchableView().contentElement.querySelector('.search-results-matches').textContent;
  }

  function dumpMatches() {
    var matches = consoleView.element
      .childTextNodes()
      .filter(node => node.parentElement.classList.contains('highlighted-search-result'))
      .map(node => node.parentElement);
    addResult('number of visible matches: ' + matches.length);
    for (var i = 0; i < matches.length; ++i) addResult('  match ' + i + ': ' + matches[i].className);
    addResult('');
  }

  var consoleView = Console.ConsoleView.ConsoleView.instance();
  var viewport = consoleView.viewport;
  const maximumViewportMessagesCount = 150;
  TestRunner.runTestSuite([
    function waitForMessages(next) {
      // NOTE: keep in sync with populateConsoleWithMessages above.
      const expectedMessageCount = 203;
      ConsoleTestRunner.waitForConsoleMessages(expectedMessageCount, next);
    },

    function assertViewportHeight(next) {
      viewport.invalidate();
      var viewportMessagesCount = viewport.lastVisibleIndex - viewport.firstVisibleIndex;
      if (viewportMessagesCount > maximumViewportMessagesCount) {
        TestRunner.addResult(
          Platform.StringUtilities.sprintf(
            "Test cannot be run because viewport can fit %d messages, while %d is the test's maximum.",
            viewportMessagesCount,
            maximumViewportMessagesCount
          )
        );
        TestRunner.completeTest();
        return;
      }
      next();
    },

    function testSearchOutsideOfViewport(next) {
      setQuery('Message', false, false, function() {
        TestRunner.addResult("Search matches: '" + matchesText() + "'");
        next();
      });
    },

    function scrollConsoleToTop(next) {
      viewport.forceScrollItemToBeFirst(0);
      addResult('first visible message index: ' + viewport.firstVisibleIndex());
      next();
    },

    function testCanJumpForward(next) {
      setQuery('MATCH', false, false, function() {
        // Find first match.
        consoleView.searchableView().handleFindNextShortcut();
        addResult('first visible message index: ' + viewport.firstVisibleIndex());

        // Find second match.
        consoleView.searchableView().handleFindNextShortcut();
        addResult('first visible message index: ' + viewport.firstVisibleIndex());

        // Find last match.
        consoleView.searchableView().handleFindNextShortcut();
        addResult('last visible message index: ' + viewport.lastVisibleIndex());
        next();
      });
    },

    function testCanJumpBackward(next) {
      setQuery('MATCH', false, false, function() {
        // Start out at the first match.
        consoleView.searchableView().handleFindNextShortcut();

        // Find last match.
        consoleView.searchableView().handleFindPreviousShortcut();
        addResult('last visible message index: ' + viewport.lastVisibleIndex());

        // Find second match.
        consoleView.searchableView().handleFindPreviousShortcut();
        addResult('first visible message index: ' + viewport.firstVisibleIndex());

        // Find first match.
        consoleView.searchableView().handleFindPreviousShortcut();
        addResult('first visible message index: ' + viewport.firstVisibleIndex());
        next();
      });
    },

    function scrollConsoleToTop(next) {
      viewport.forceScrollItemToBeFirst(0);
      addResult('first visible message index: ' + viewport.firstVisibleIndex());
      next();
    },

    function testCanMarkCurrentMatch(next) {
      function addCurrentMarked() {
        var matches = document.querySelectorAll('.highlighted-search-result');
        addResult('number of visible matches: ' + matches.length);
        for (var i = 0; i < matches.length; ++i) addResult('match ' + i + ': ' + matches[i].className);
      }

      setQuery('MATCH', false, false, function() {
        // Find first match.
        consoleView.searchableView().handleFindNextShortcut();
        addCurrentMarked();

        // Find second match.
        consoleView.searchableView().handleFindNextShortcut();
        addCurrentMarked();

        next();
      });
    },

    function testCanJumpForwardBetweenTreeElementMatches(next) {
      function dumpElements(callback) {
        consoleView.searchableView().handleFindNextShortcut();
        var currentResultElem = consoleView.element
          .childTextNodes()
          .filter(node => node.parentElement.classList.contains('current-search-result'))[0];
        addResult('matched tree element: ' + currentResultElem.textContent);
        callback();
      }

      function searchField1(callback) {
        // Find first match.
        setQuery('field_1', false, false, dumpElements.bind(null, callback));
      }

      function searchField199(callback) {
        // Find last match.
        setQuery('field_199', false, false, dumpElements.bind(null, callback));
      }

      ConsoleTestRunner.expandConsoleMessages(searchField1.bind(null, searchField199.bind(null, next)));
    },

    function testCaseInsensitiveRegex(next) {
      setQuery('. MATCH', true, false, function() {
        consoleView.searchableView().handleFindNextShortcut();
        dumpMatches();
        next();
      });
    },

    function testCaseSensitiveTextWithoutMatches(next) {
      setQuery('match', false, true, function() {
        consoleView.searchableView().handleFindNextShortcut();
        dumpMatches();
        next();
      });
    },

    function testCaseSensitiveTextWithMatches(next) {
      setQuery('MATCH', false, true, function() {
        consoleView.searchableView().handleFindNextShortcut();
        dumpMatches();
        next();
      });
    },

    function testCaseSensitiveRegexWithoutMatches(next) {
      setQuery('. match', true, true, function() {
        consoleView.searchableView().handleFindNextShortcut();
        dumpMatches();
        next();
      });
    },

    function testCaseSensitiveRegexWithMatches(next) {
      setQuery('. MATCH', true, true, function() {
        consoleView.searchableView().handleFindNextShortcut();
        dumpMatches();
        next();
      });
    }
  ]);
})();
