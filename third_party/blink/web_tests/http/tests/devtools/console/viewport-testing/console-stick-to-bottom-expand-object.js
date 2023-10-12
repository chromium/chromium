// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(`Verifies viewport stick-to-bottom behavior when prompt has space below editable area.\n`);
  await TestRunner.showPanel('console');
  await ConsoleTestRunner.waitUntilConsoleEditorLoaded();
  ConsoleTestRunner.fixConsoleViewportDimensions(600, 200);
  await TestRunner.evaluateInPagePromise(`
      for (var i = 0; i < 150; ++i)
        console.log({id: "#" + i, anotherKey: true, toGrowExpandedHeight: true});

      //# sourceURL=console-viewport-stick-to-bottom-expand-object.js
    `);
  await ConsoleTestRunner.waitForConsoleMessagesPromise(150);
  await ConsoleTestRunner.waitForPendingViewportUpdates();

  const consoleView = Console.ConsoleView.ConsoleView.instance();
  const viewport = consoleView.viewport;

  TestRunner.runTestSuite([
    async function testExpandLastVisibleObjectRemainsInView(next) {
      const index = consoleView.visibleViewMessages.length - 1;
      forceSelect(index);
      dumpInfo();

      TestRunner.addResult('Expanding object');
      const objectSection = consoleView.visibleViewMessages[index].selectableChildren[0];
      objectSection.objectTreeElement().expand();
      await ConsoleTestRunner.waitForRemoteObjectsConsoleMessagesPromise();
      dumpInfo();

      TestRunner.addResult('Collapsing object');
      objectSection.objectTreeElement().collapse();
      dumpInfo();

      next();
    },

    async function testExpandFirstVisibleObjectRemainsInView(next) {
      const index = viewport.firstVisibleIndex() + 1;   // add 1 for first "fully visible" item
      forceSelect(index);
      dumpInfo();

      TestRunner.addResult('Expanding object');
      const objectSection = consoleView.visibleViewMessages[index].selectableChildren[0];
      objectSection.objectTreeElement().expand();
      dumpInfo();

      TestRunner.addResult('Collapsing object');
      objectSection.objectTreeElement().collapse();
      dumpInfo();

      next();
    },
  ]);

  function dumpInfo() {
    viewport.refresh();
    let infoText =
      'Is at bottom: ' + TestRunner.isScrolledToBottom(viewport.element) + ', should stick: ' + viewport.stickToBottom();
    const selectedElement = viewport.renderedElementAt(viewport.virtualSelectedIndex);
    if (selectedElement) {
      const selectedRect = selectedElement.getBoundingClientRect();
      const viewportRect = viewport.element.getBoundingClientRect();
      const fullyVisible = (selectedRect.top + 2.5 >= viewportRect.top && selectedRect.bottom - 2.5 <= viewportRect.bottom);
      infoText += ', selected element is fully visible? ' + fullyVisible;
    }
    TestRunner.addResult(infoText);
  }

  function forceSelect(index) {
    TestRunner.addResult(`\nForce selecting index ${index}`);
    viewport.virtualSelectedIndex = index;
    viewport.contentElement().focus();
    viewport.updateFocusedItem();
  }
})();
