// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadTestModule('axe_core_test_runner');
  await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise('../sources/debugger-breakpoints/resources/dom-breakpoints.html');

  TestRunner.addResult('Adding global listener.');
  await TestRunner.evaluateInPagePromise('window.addEventListener(\'touchstart\', () => console.log);');
  await UI.viewManager.showView('sources.globalListeners');
  const globalListenersPane = BrowserDebugger.ObjectEventListenersSidebarPane.instance();
  const eventListenersView = globalListenersPane.eventListenersView;

  TestRunner.addResult('Dumping event listeners view:');
  await ElementsTestRunner.expandAndDumpEventListenersPromise(eventListenersView);
  TestRunner.addResult('Running the axe-core linter on the global listeners pane.');
  await AxeCoreTestRunner.runValidation(globalListenersPane.contentElement);
  TestRunner.completeTest();
})();
