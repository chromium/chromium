// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(`Tests that console dir makes messages expandable only when necessary.\n`);

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    // Primitive values should not get properties section or expand arrow.
    console.dir(true);
    console.dir(new Boolean(true));

    console.dir("foo");
    console.dir(new String("foo"));

    //# sourceURL=console-dir-primitives.js
  `);

  var consoleView = Console.ConsoleView.ConsoleView.instance();
  var viewMessages = consoleView.visibleViewMessages;
  for (var i = 0; i < viewMessages.length; ++i) {
    var messageElement = viewMessages[i].element();
    // Console messages contain live locations.
    await TestRunner.waitForPendingLiveLocationUpdates();
    TestRunner.addResult('Message text: ' + messageElement.deepTextContent());
    if (messageElement.querySelector('.console-view-object-properties-section'))
      TestRunner.addResult('Message is expandable');
    else
      TestRunner.addResult('Message is not expandable');
    TestRunner.addResult("");
  }
  TestRunner.completeTest();
})();
