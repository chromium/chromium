// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  // This await is necessary for evaluateInPagePromise to produce accurate line numbers.
  await TestRunner.addResult(`Tests WebInspector.RemoveObject.setPropertyValue implementation.\n`);
  await TestRunner.evaluateInPagePromise(`
      var object1 = { foo: 1 };
      var object2 = { bar: 2 };

      function dumpObject(label)
      {
          console.log("===== " + label + " =====");
          console.log(JSON.stringify(object1, replacer));
          console.log("");

          function replacer(key, value)
          {
              if (typeof value === "number" && !isFinite(value))
                  return String(value);
              return value;
          }
      }

      function checkNegativeZero()
      {
          console.log("===== Checking negative zero =====");
          console.log("1/-0 = " + (1 / object1.foo));
      }
  `);

  var obj1, obj2;
  var nameFoo = SDK.RemoteObject.RemoteObject.toCallArgument('foo');

  TestRunner.runTestSuite([
    function testSetUp(next) {
      TestRunner.evaluateInPage('dumpObject(\'Initial\')', step0);

      async function step0() {
        var result = await TestRunner.RuntimeAgent.evaluate('object1');
        obj1 = TestRunner.runtimeModel.createRemoteObject(result);
        result = await TestRunner.RuntimeAgent.evaluate('object2');
        obj2 = TestRunner.runtimeModel.createRemoteObject(result);
        next();
      }
    },

    async function testSetPrimitive(next) {
      await obj1.setPropertyValue(nameFoo, '2');
      TestRunner.evaluateInPage('dumpObject(\'Set primitive\')', next);
    },

    async function testSetHandle(next) {
      await obj1.setPropertyValue(nameFoo, 'object2');
      TestRunner.evaluateInPage('dumpObject(\'Set handle\')', next);
    },

    async function testSetUndefined(next) {
      await obj1.setPropertyValue(nameFoo, 'undefined');
      TestRunner.evaluateInPage('dumpObject(\'Set undefined\')', next);
    },

    async function testSetZero(next) {
      await obj1.setPropertyValue(nameFoo, '0');
      TestRunner.evaluateInPage('dumpObject(\'Set zero\')', next);
    },

    async function testSetNull(next) {
      await obj1.setPropertyValue(nameFoo, 'null');
      TestRunner.evaluateInPage('dumpObject(\'Set null\')', next);
    },

    async function testSetEmptyString(next) {
      await obj1.setPropertyValue(nameFoo, '""');
      TestRunner.evaluateInPage('dumpObject(\'Set empty string\')', next);
    },

    async function testSetException(next) {
      var error = await obj1.setPropertyValue(nameFoo, 'throw \'exception\'');
      TestRunner.addResult(error);
      TestRunner.evaluateInPage('dumpObject(\'Set exception\')', next);
    },

    async function testSetNonFiniteNumbers(next) {
      await obj1.setPropertyValue(nameFoo, 'NaN');
      await obj1.setPropertyValue(SDK.RemoteObject.RemoteObject.toCallArgument('foo1'), 'Infinity');
      await obj1.setPropertyValue(SDK.RemoteObject.RemoteObject.toCallArgument('foo2'), '-Infinity');
      TestRunner.evaluateInPage('dumpObject(\'Set non-finite numbers\')', next);
    },

    async function testNegativeZero(next) {
      await obj1.setPropertyValue(nameFoo, '1/-Infinity');
      TestRunner.evaluateInPage('checkNegativeZero()', next);
    },

    function testReleaseObjectIsCalled(next) {
      // If failed, this test will time out.
      TestRunner.addSniffer(TestRunner.RuntimeAgent, 'invoke_releaseObject', async () => {
        await ConsoleTestRunner.dumpConsoleMessages();
        next();
      });
      obj1.setPropertyValue(nameFoo, '[1,2,3]');
    }
  ]);
})();
