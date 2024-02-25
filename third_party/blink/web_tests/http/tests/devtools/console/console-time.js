// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`console.time / console.timeEnd tests.\n`);

  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    function testFunction()
    {
      console.time();
      console.timeEnd();
      console.time("42");
      console.timeEnd("42");
      console.time(239)
      console.timeEnd(239);
      console.time({});
      console.timeEnd({});
    }
  `);

  ConsoleTestRunner.waitUntilNthMessageReceived(4, dumpMessagesAndCompleTest);
  TestRunner.evaluateInPage('testFunction()');

  async function dumpMessagesAndCompleTest() {
    var messages = await ConsoleTestRunner.dumpConsoleMessagesIntoArray();
    messages = messages.map(message => message.replace(/\d+\.\d+ ?ms/, '<time>'));
    TestRunner.addResults(messages);
    TestRunner.completeTest();
  }
})();
