// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that overriding Function.prototype.bind does not break inspector.\n`);

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
     var foo = 'fooValue';
    Function.prototype.bind = function () { throw ":P"; };
  `);

  ConsoleTestRunner.evaluateInConsole('foo', step1);

  async function step1() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
