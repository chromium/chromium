// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Verifies viewport stick-to-bottom behavior when Console is opened.\n`);

  // Log a ton of messages before opening console.
  await TestRunner.evaluateInPagePromise(`
      for (var i = 0; i < 150; ++i)
        console.log("Message #" + i);

      //# sourceURL=console-viewport-stick-to-bottom-onload.js
    `);
  await TestRunner.loadLegacyModule('console');
  await TestRunner.showPanel('console');
  await ConsoleTestRunner.waitUntilConsoleEditorLoaded();

  var viewport = Console.ConsoleView.instance().viewport;
  ConsoleTestRunner.waitForConsoleMessagesPromise(150);
  await ConsoleTestRunner.waitForPendingViewportUpdates();

  TestRunner.addResult(
    'Is at bottom: ' + TestRunner.isScrolledToBottom(viewport.element) + ', should stick: ' + viewport.stickToBottom());
  TestRunner.completeTest();
})();
