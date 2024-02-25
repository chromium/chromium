// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(`Tests that console copies tree outline messages properly.\n`);

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    var anObject = {
        foo: 1,
        bar: "string"
    };
    console.log(anObject);
  `);

  ConsoleTestRunner.fixConsoleViewportDimensions(600, 200);
  var consoleView = Console.ConsoleView.ConsoleView.instance();
  var viewport = consoleView.viewport;

  TestRunner.runTestSuite([async function testSelectAll(next) {
    viewport.forceScrollItemToBeFirst(0);

    // Set some initial selection in console.
    var base = consoleView.itemElement(0).element();
    // Console messages contain live locations.
    await TestRunner.waitForPendingLiveLocationUpdates();
    window.getSelection().setBaseAndExtent(base, 0, base, 1);

    // Try to select all messages.
    document.execCommand('selectAll');

    var text = viewport.selectedText();
    TestRunner.addResult('Selected text: ' + text);
    next();
  }]);
})();
