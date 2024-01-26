// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';
import * as BrowserDebugger from 'devtools/panels/browser_debugger/browser_debugger.js';

(async function () {
  await TestRunner.showPanel('sources');

  await UI.ViewManager.ViewManager.instance().showView('sources.event-listener-breakpoints');
  const eventListenerWidget = BrowserDebugger.EventListenerBreakpointsSidebarPane.EventListenerBreakpointsSidebarPane.instance();
  TestRunner.addResult('Setting event listener breakpoints.');
  const {checkbox, element} = eventListenerWidget.categories.get('Animation');
  element.revealAndSelect();
  checkbox.click();

  TestRunner.addResult('Dumping Animation category.');
  TestRunner.addResult(element.listItemElement.deepTextContent());
  TestRunner.addResult('Running the axe-core linter on the Animation category of the event listener breakpoints pane.');
  await AxeCoreTestRunner.runValidation(element.listItemElement);

  TestRunner.completeTest();
})();
