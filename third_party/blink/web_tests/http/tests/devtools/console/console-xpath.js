// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(`Tests $x for iterator and non-iterator types.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.loadHTML(`
      <a href="http://chromium.org"></a>
      <p id="test"></p>
  `);

  TestRunner.addSniffer(
      Console.ConsoleViewMessage.ConsoleViewMessage.prototype, 'formattedParameterAsNodeForTest', formattedParameter, true);
  ConsoleTestRunner.addConsoleViewSniffer(messageSniffer, true);

  await ConsoleTestRunner.evaluateInConsolePromise('$x(\'42\')');                           // number
  await ConsoleTestRunner.evaluateInConsolePromise('$x(\'name(/html)\')');                  // string
  await ConsoleTestRunner.evaluateInConsolePromise('$x(\'not(42)\')');                      // boolean
  await ConsoleTestRunner.evaluateInConsolePromise('$x(\'/html/body/p\').length');          // node iterator
  await ConsoleTestRunner.evaluateInConsolePromise('$x(\'//a/@href\')[0]');                 // href, should not throw
  await ConsoleTestRunner.evaluateInConsolePromise('$x(\'./a/@href\', document.body)[0]');  // relative to document.body selector
  await ConsoleTestRunner.evaluateInConsolePromise('$x(\'./a@href\', document.body)');      // incorrect selector, shouldn't crash
  TestRunner.evaluateInPage('console.log(\'complete\')');                      // node iterator

  var completeMessageReceived = false;
  function messageSniffer(uiMessage) {
    if (uiMessage.element().deepTextContent().indexOf('complete') !== -1) {
      completeMessageReceived = true;
      maybeCompleteTest();
    }
  }

  var waitForParameteres = 2;
  function formattedParameter() {
    waitForParameteres--;
    maybeCompleteTest();
  }

  async function maybeCompleteTest() {
    if (!waitForParameteres && completeMessageReceived) {
      await ConsoleTestRunner.dumpConsoleMessages();
      TestRunner.completeTest();
    }
  }
})();
