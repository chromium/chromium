// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Tests that watches pane renders errors in red.\n`);
  await TestRunner.loadLegacyModule('sources');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      var foo = 123
  `);

  var watchExpressionsPane = Sources.WatchExpressionsSidebarPane.instance();
  UI.panels.sources.sidebarPaneStack.showView(UI.panels.sources.watchSidebarPane).then(() => {
    watchExpressionsPane.doUpdate();
    watchExpressionsPane.createWatchExpression('#$%');
    watchExpressionsPane.saveExpressions();
    TestRunner.deprecatedRunAfterPendingDispatches(step1);
  });


  function step1() {
    TestRunner.addResult(
        watchExpressionsPane.contentElement.deepTextContent().indexOf('<not available>') !== -1 ? 'SUCCESS' : 'FAILED');

    // Clear watch expressions after execution.
    watchExpressionsPane.deleteAllButtonClicked();
    TestRunner.completeTest();
  }
})();
