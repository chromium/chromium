// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise('../sources/debugger-breakpoints/resources/dom-breakpoints.html');

  TestRunner.addResult('Adding global listener.');
  await TestRunner.evaluateInPagePromise(`window.addEventListener('touchstart', () => console.log);`);
  await UI.viewManager.showView('sources.globalListeners');
  const globalListenersPane = self.runtime.sharedInstance(BrowserDebugger.ObjectEventListenersSidebarPane);
  const eventListenersView = globalListenersPane._eventListenersView;

  TestRunner.addResult('Dumping event listeners view:');
  await ElementsTestRunner.expandAndDumpEventListenersPromise(eventListenersView);
  TestRunner.addResult('Running the axe-core linter on the global listeners pane.');
  await AxeCoreTestRunner.runValidation(globalListenersPane.contentElement);
  TestRunner.completeTest();
})();
