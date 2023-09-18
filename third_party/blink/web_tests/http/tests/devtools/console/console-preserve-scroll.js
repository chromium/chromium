// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as UIModule from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`Tests that console preserves scroll position when switching away.\n`);
  await TestRunner.loadLegacyModule('console');
  await TestRunner.showPanel('console');
  // Do not use ConsoleTestRunner.fixConsoleViewportDimensions because fixing the height will affect
  // tests that may cause scrolling while the console moves into/out of the drawer.
  UI.inspectorView.element.style.maxHeight = '600px';
  await ConsoleTestRunner.waitUntilConsoleEditorLoaded();
  await TestRunner.evaluateInPagePromise(`
    for (var i = 0; i < 100; i++)
      console.log('foo' + i);
  `);
  await ConsoleTestRunner.waitForConsoleMessagesPromise(100);

  var consoleView = Console.ConsoleView.instance();
  var viewport = consoleView.viewport;
  viewport.setStickToBottom(false);
  // Avoid flakiness by ensuring that messages in visibleViewMessages are in DOM.
  viewport.invalidate();
  viewport.element.scrollTop = 10;
  dumpScrollTop();

  UI.inspectorView._tabbedPane.addEventListener(UIModule.TabbedPane.Events.TabSelected, () => {
    TestRunner.addResult('Panel ' + UI.inspectorView._tabbedPane._currentTab.id + ' was opened.');
  });

  TestRunner.runTestSuite([
    async function testSwitchToAnotherPanel(next) {
      await TestRunner.showPanel('sources');
      await TestRunner.showPanel('console');
      dumpScrollTop();
      next();
    },

    async function testClickLinkToRevealAnotherPanel(next) {
      // Ordering is important here, as accessing the element the first time around
      // triggers live location creation and updates which we need to await properly.
      const element = consoleView.visibleViewMessages[0]._element;
      await TestRunner.waitForPendingLiveLocationUpdates();
      element.querySelector('.devtools-link').click();
      await UI.inspectorView._tabbedPane.once(UIModule.TabbedPane.Events.TabSelected);
      await TestRunner.showPanel('console');
      dumpScrollTop();
      next();
    },

    async function testConsolePanelToDrawer(next) {
      await TestRunner.showPanel('console');
      await showDrawerPromise();
      TestRunner.addResult('Drawer panel set to ' + UI.inspectorView._drawerTabbedPane._currentTab.id);
      await TestRunner.showPanel('sources');
      dumpScrollTop();
      await TestRunner.showPanel('console');
      dumpScrollTop();
      next();
    },

    async function testCloseDrawerFromConsolePanelAndOpenFromAnotherPanel(next) {
      await TestRunner.showPanel('console');
      TestRunner.addSniffer(UIModule.SplitWidget.SplitWidget.prototype, '_showFinishedForTest', async () => {
        await TestRunner.showPanel('sources');
        await showDrawerPromise();
        TestRunner.addResult('Drawer panel set to ' + UI.inspectorView._drawerTabbedPane._currentTab.id);
        dumpScrollTop();
        next();
      });
      // Close the drawer with animation.
      UI.inspectorView._drawerSplitWidget.hideSidebar(true /* animate */);
    }
  ]);

  function dumpScrollTop() {
    TestRunner.addResult(`Console scrollTop: ${viewport.element.scrollTop}`);
  }

  async function showDrawerPromise() {
    // Restoring scroll positions may occur during materialization, which is
    // done asynchronously for TabbedPane contents.
    return new Promise((resolve, reject) => {
      UI.inspectorView._showDrawer(true);
      TestRunner.addSniffer(UIModule.ViewManager.ContainerWidget.prototype, '_wasShownForTest', resolve);
    });
  }
})();
