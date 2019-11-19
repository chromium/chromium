// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise('../sources/debugger-breakpoints/resources/dom-breakpoints.html');

  await UI.viewManager.showView('sources.watch');
  TestRunner.addResult('Adding watch expression.');
  const watchPane = self.runtime.sharedInstance(Sources.WatchExpressionsSidebarPane);
  watchPane.doUpdate();
  TestRunner.addResult('Running the axe-core linter on the empty watch pane.');
  await AxeCoreTestRunner.runValidation(watchPane.contentElement);

  const watchExpression = watchPane._createWatchExpression('2 + 2 === 5');
  await TestRunner.addSnifferPromise(Sources.WatchExpression.prototype, '_createWatchExpression');

  const watchExpressionElement = watchExpression.treeElement().listItemElement;
  TestRunner.addResult(`Watch expression text content: ${watchExpressionElement.deepTextContent()}`);
  TestRunner.addResult('Running the axe-core linter on the watch expression.');
  await AxeCoreTestRunner.runValidation(watchExpressionElement);
  TestRunner.completeTest();
})();
