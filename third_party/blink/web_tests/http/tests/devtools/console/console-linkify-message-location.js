// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Test that console.log() would linkify its location in respect with ignore-listing.\n`);

  await TestRunner.loadLegacyModule('console');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    function foo()
    {
      console.trace(239);
    }
    //# sourceURL=foo.js
  `);
  await TestRunner.evaluateInPagePromise(`
    function boo()
    {
      foo();
    }
    //# sourceURL=boo.js
  `);

  TestRunner.evaluateInPage('boo()', step1);

  async function step1() {
    await dumpConsoleMessageURLs();

    TestRunner.addSniffer(Bindings.IgnoreListManager.prototype, 'patternChangeFinishedForTests', step2);
    var frameworkRegexString = 'foo\\.js';
    Common.settingForTest('skipStackFramesPattern').set(frameworkRegexString);
  }

  async function step2() {
    await dumpConsoleMessageURLs();
    TestRunner.addSniffer(Bindings.IgnoreListManager.prototype, 'patternChangeFinishedForTests', step3);
    var frameworkRegexString = 'foo\\.js|boo\\.js';
    Common.settingForTest('skipStackFramesPattern').set(frameworkRegexString);
  }

  async function step3() {
    await dumpConsoleMessageURLs();
    TestRunner.addSniffer(Bindings.IgnoreListManager.prototype, 'patternChangeFinishedForTests', step4);
    var frameworkRegexString = '';
    Common.settingForTest('skipStackFramesPattern').set(frameworkRegexString);
  }

  async function step4() {
    await dumpConsoleMessageURLs();
    TestRunner.completeTest();
  }

  async function dumpConsoleMessageURLs() {
    var messages = Console.ConsoleView.instance().visibleViewMessages;
    for (var i = 0; i < messages.length; ++i) {
      // Ordering is important here. Retrieveing the message element the first time triggers
      // live location creation and updates, which we need to await for correct locations.
      var element = messages[i].element();
      await TestRunner.waitForPendingLiveLocationUpdates();
      var anchor = element.querySelector('.console-message-anchor');
      TestRunner.addResult(anchor.textContent.replace(/VM\d+/g, 'VM'));
    }
  }
})();
