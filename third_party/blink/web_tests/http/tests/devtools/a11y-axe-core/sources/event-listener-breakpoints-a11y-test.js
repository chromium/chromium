// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AxeCoreTestRunner} from 'axe_core_test_runner';
import * as BrowserDebugger from 'devtools/panels/browser_debugger/browser_debugger.js';
import * as Common from 'devtools/core/common/common.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
import {TestRunner} from 'test_runner';

(async function() {
  await TestRunner.showPanel('sources');

  Common.Settings.Settings.instance().moduleSetting('sidebar-position').set('right');
  await UI.ViewManager.ViewManager.instance().showView(
      'sources.event-listener-breakpoints');
  const eventListenerWidget =
      BrowserDebugger.EventListenerBreakpointsSidebarPane
          .EventListenerBreakpointsSidebarPane.instance();
  TestRunner.addResult('Setting event listener breakpoints.');
  const breakpoints = eventListenerWidget.categories.get('animation');
  breakpoints?.forEach(b => b.setEnabled(true));
  eventListenerWidget.requestUpdate();
  await eventListenerWidget.updateComplete;
  const element =
      eventListenerWidget.contentElement.querySelector('devtools-tree')
          ?.shadowRoot?.querySelector('li:has([title="Animation"])')


  TestRunner.addResult('Dumping Animation category.');
  TestRunner.addResult(element.deepTextContent());
  TestRunner.addResult(
      'Running the axe-core linter on the Animation category of the event listener breakpoints pane.');
  await AxeCoreTestRunner.runValidation(element);

  TestRunner.completeTest();
})();
