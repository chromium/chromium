// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult('Tests that console properly displays information about ES6 Symbols.\n');

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    function logSymbolToConsoleWithError()
    {
        Symbol.prototype.toString = function (arg) { throw new Error(); };
        var smb = Symbol();
        console.log(smb);
    }
  `);

  TestRunner.evaluateInPage('logSymbolToConsoleWithError()', complete);

  async function complete() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
