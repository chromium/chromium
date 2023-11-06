// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  // This await is necessary for evaluateInPagePromise to produce accurate line numbers.
  await TestRunner.addResult(`Tests editing Symbol properties.\n`);
  await TestRunner.evaluateInPagePromise(`
      var object1 = { foo: 1 };
      var symbol1 = Symbol("a");
      object1[symbol1] = 2;

      function dumpSymbolProperty(label)
      {
          console.log("===== " + label + " =====");
          console.log(object1[symbol1]);
          console.log("");
      }
  `);

  var obj1, name;

  async function dumpAndClearConsoleMessages() {
    await ConsoleTestRunner.dumpConsoleMessages();
    Console.ConsoleView.ConsoleView.clearConsole();
  }

  TestRunner.runTestSuite([
    function testSetUp(next) {
      TestRunner.evaluateInPage('dumpSymbolProperty(\'Initial\')', step0);

      async function step0() {
        var result = await TestRunner.RuntimeAgent.evaluate('object1');
        obj1 = TestRunner.runtimeModel.createRemoteObject(result);
        result = await TestRunner.RuntimeAgent.evaluate('symbol1');
        name = SDK.RemoteObject.RemoteObject.toCallArgument(TestRunner.runtimeModel.createRemoteObject(result));
        await dumpAndClearConsoleMessages();
        next();
      }
    },

    async function testSetSymbolPropertyValue(next) {
      await obj1.setPropertyValue(name, '3');
      await TestRunner.evaluateInPage('dumpSymbolProperty(\'Set property\')');
      await dumpAndClearConsoleMessages();
      next();
    },

    async function testDeleteSymbolProperty(next) {
      await obj1.deleteProperty(name);
      await TestRunner.evaluateInPagePromise('dumpSymbolProperty(\'Delete property\')');
      await dumpAndClearConsoleMessages();
      next();
    }
  ]);
})();
