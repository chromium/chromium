// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(`Tests that console logging dumps proper messages with broken Unicode.\n`);

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    var str = "  \uD835\uDC14\uD835\uDC0D\uD835\uDC08\uD835\uDC02\uD835\uDC0E\uD835\uDC03\uD835\uDC04"; // "  UNICODE"
    var brokenSurrogate = str.substring(0, str.length - 1);
    var obj = { foo: brokenSurrogate };
    obj[brokenSurrogate] = "foo";
  `);

  ConsoleTestRunner.evaluateInConsole('obj');
  ConsoleTestRunner.evaluateInConsole('[obj]');
  ConsoleTestRunner.evaluateInConsole('obj.foo');
  ConsoleTestRunner.evaluateInConsole('[obj.foo]');
  TestRunner.deprecatedRunAfterPendingDispatches(step1);

  function step1() {
    ConsoleTestRunner.waitForRemoteObjectsConsoleMessages(step2);
  }

  function step2() {
    ConsoleTestRunner.expandConsoleMessages(step3);
  }

  function step3() {
    ConsoleTestRunner.expandConsoleMessages(step4, expandFirstArrayIndexFilter);
  }

  function step4() {
    TestRunner.evaluateInPage('obj.foo', step5);
  }

  function step5(result) {
    var text = result;
    TestRunner.assertEquals(15, text.length, 'text length');
    // It's important that this last character in |text| is an unbalanced UTF16
    // low surrogate (|text| is invalid UTF16, allowed by Javascript), as
    // opposed to some replacement character that came from transcoding to UTF8
    // and back to valid UTF16.
    TestRunner.assertEquals('\uD835', text[text.length - 1]);

    ConsoleTestRunner.disableConsoleViewport();
    var viewMessages = Console.ConsoleView.ConsoleView.instance().visibleViewMessages;

    for (var i = 0; i < viewMessages.length; ++i) {
      var node = viewMessages[i].contentElement();
      TestRunner.addResult(node.textContent);
    }

    TestRunner.completeTest();
  }

  function expandFirstArrayIndexFilter(treeElement) {
    var propertyName = treeElement.nameElement && treeElement.nameElement.textContent;
    return propertyName === '0';
  }
})();
