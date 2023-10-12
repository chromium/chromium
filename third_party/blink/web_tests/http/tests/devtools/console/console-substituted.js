// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that evaluate in console works even if window.console is substituted or deleted. Bug 53072\n`);
  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
      function deleteConsole()
      {
          window.console = undefined;
      }

      function substituteConsole()
      {
          Object.defineProperty(window, "__commandLineAPI", { enumerable: false, configurable: false, get: function() { throw "Substituted" }});
      }
  `);

  ConsoleTestRunner.evaluateInConsole('deleteConsole()', step1);

  function step1() {
    ConsoleTestRunner.evaluateInConsole('1', step2);
  }

  function step2(result) {
    TestRunner.addResult(result);
    ConsoleTestRunner.evaluateInConsole('substituteConsole()', step3);
  }

  function step3(result) {
    ConsoleTestRunner.evaluateInConsole('2', step4);
  }

  function step4(result) {
    TestRunner.addResult(result);
    TestRunner.completeTest();
  }
})();
