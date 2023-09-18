// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as UIModule from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`Tests tabbed pane tabs to show calculation.\n`);

  function createFakeTab(title, width) {
    function toString() {
      return title;
    }
    return {
      width: function() {
        return width;
      },
      title: title,
      toString: toString
    };
  }

  var tabbedPane = new UIModule.TabbedPane.TabbedPane();
  tabbedPane.setAllowTabReorder(true, true);

  var dropDownButtonMeasuredWidth = 10;
  function getTabsToShowAndDumpResults(tabsOrdered, tabsHistory, totalWidth) {
    var tabsToShowIndexes = UIModule.TabbedPane.TabbedPane.prototype.tabsToShowIndexes.call(
        tabbedPane, tabsOrdered, tabsHistory, totalWidth, dropDownButtonMeasuredWidth);
    TestRunner.addResult('    tabsToShowIndexes = [' + String(tabsToShowIndexes) + ']');
  }

  function testWidthsAndHistory(widths, history, totalWidth) {
    var tabsOrdered = [];
    var tabsHistory = [];
    for (var i = 0; i < widths.length; i++)
      tabsOrdered.push(createFakeTab('tab ' + i, widths[i]));
    for (var i = 0; i < history.length; i++)
      tabsHistory.push(tabsOrdered[history[i]]);
    TestRunner.addResult('Running tabs to show test:');
    TestRunner.addResult('    widths = [' + String(widths) + ']');
    TestRunner.addResult('    tabsHistory = [' + String(tabsHistory) + ']');
    TestRunner.addResult(
        '    totalWidth = ' + totalWidth + ', dropDownButtonMeasuredWidth = ' + dropDownButtonMeasuredWidth);
    getTabsToShowAndDumpResults(tabsOrdered, tabsHistory, totalWidth);
  }

  function testWithDifferentTotalWidths(widths, history) {
    testWidthsAndHistory(widths, history, 370);
    testWidthsAndHistory(widths, history, 360);
    testWidthsAndHistory(widths, history, 350);
    testWidthsAndHistory(widths, history, 300);
    testWidthsAndHistory(widths, history, 250);
    testWidthsAndHistory(widths, history, 200);
    testWidthsAndHistory(widths, history, 150);
    testWidthsAndHistory(widths, history, 100);
    testWidthsAndHistory(widths, history, 60);
    testWidthsAndHistory(widths, history, 50);
    TestRunner.addResult('');
  }

  var widths = [50, 50, 60, 60, 70, 70];
  testWithDifferentTotalWidths(widths, [0, 1, 2, 3, 4, 5]);
  testWithDifferentTotalWidths(widths, [5, 4, 3, 2, 1, 0]);
  testWithDifferentTotalWidths(widths, [0, 2, 4, 1, 3, 5]);
  testWithDifferentTotalWidths(widths, [5, 3, 1, 4, 2, 0]);

  tabbedPane.setAllowTabReorder(false);

  testWithDifferentTotalWidths(widths, [0, 1, 2, 3, 4, 5]);
  testWithDifferentTotalWidths(widths, [5, 4, 3, 2, 1, 0]);
  testWithDifferentTotalWidths(widths, [0, 2, 4, 1, 3, 5]);
  testWithDifferentTotalWidths(widths, [5, 3, 1, 4, 2, 0]);

  TestRunner.completeTest();
})();
