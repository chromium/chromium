// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';
import * as BrowserDebugger from 'devtools/panels/browser_debugger/browser_debugger.js';

(async function() {
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise('../sources/debugger-breakpoints/resources/dom-breakpoints.html');

  TestRunner.addResult('Adding global listener.');
  await TestRunner.evaluateInPagePromise('window.addEventListener(\'touchstart\', () => console.log);');
  await UI.ViewManager.ViewManager.instance().showView('sources.global-listeners');
  const globalListenersPane = UI.Context.Context.instance().flavor(BrowserDebugger.ObjectEventListenersSidebarPane.ObjectEventListenersSidebarPane);
  const eventListenersView = globalListenersPane.eventListenersView;

  TestRunner.addResult('Dumping event listeners view:');
  await ElementsTestRunner.expandAndDumpEventListenersPromise(eventListenersView);
  TestRunner.addResult('Running the axe-core linter on the global listeners pane.');
  await AxeCoreTestRunner.runValidation(globalListenersPane.contentElement);
  TestRunner.completeTest();
})();
