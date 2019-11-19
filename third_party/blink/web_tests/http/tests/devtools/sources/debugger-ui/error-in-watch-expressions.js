// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that watches pane renders errors in red.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      var foo = 123
  `);

  var watchExpressionsPane = self.runtime.sharedInstance(Sources.WatchExpressionsSidebarPane);
  UI.panels.sources._sidebarPaneStack.showView(UI.panels.sources._watchSidebarPane).then(() => {
    watchExpressionsPane.doUpdate();
    watchExpressionsPane._createWatchExpression('#$%');
    watchExpressionsPane._saveExpressions();
    TestRunner.deprecatedRunAfterPendingDispatches(step1);
  });


  function step1() {
    TestRunner.addResult(
        watchExpressionsPane.contentElement.deepTextContent().indexOf('<not available>') !== -1 ? 'SUCCESS' : 'FAILED');

    // Clear watch expressions after execution.
    watchExpressionsPane._deleteAllButtonClicked();
    TestRunner.completeTest();
  }
})();
