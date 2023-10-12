// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that overriding global methods (like Array.prototype.push, Math.max) will not break the inspector.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      var originalError = window.Error;

      (function() {
          var originalFunctionCall = Function.prototype.call;
          var originalFunctionApply = Function.prototype.apply;

          var overriddenFunctionCall = function() {
              original();
              console.error("FAIL: Function.prototype.call should not be called!");
              var result = originalFunctionCall.apply(this, arguments);
              overridden();
              return result;
          }

          var overriddenFunctionApply = function(thisArg, argsArray) {
              original();
              console.error("FAIL: Function.prototype.apply should not be called!");
              var result = originalFunctionApply.call(this, thisArg, argsArray);
              overridden();
              return result;
          }

          function original()
          {
              Function.prototype.call = originalFunctionCall;
              Function.prototype.apply = originalFunctionApply;
          }

          function overridden()
          {
              Function.prototype.call = overriddenFunctionCall;
              Function.prototype.apply = overriddenFunctionApply;
          }

          overridden();
      })();

      function throwGetter()
      {
         console.error("FAIL: Should not be called!");
         throw new Error("FAIL");
      }

      function testOverriddenArrayPushAndMathMax()
      {
          Object.defineProperty(Array.prototype, "push", {
              get: throwGetter
          });
          Object.defineProperty(Math, "max", {
              get: throwGetter
          });
          return [1, 2, 3];
      }

      function testOverriddenConstructorName()
      {
          var obj = {};
          obj.constructor = { name: "foo" };
          return obj;
      }

      function testThrowConstructorName()
      {
          var obj = {};
          Object.defineProperty(obj, "constructor", {
              get: throwGetter
          });
          return obj;
      }

      function testOverriddenIsFinite()
      {
          window.isFinite = throwGetter;
          var out;
          (function() {
              out = arguments;
          })("arg1", "arg2");
          return out;
      }

      function testOverriddenError()
      {
          window.Error = 42;
          return { result: "PASS" };
      }

      function restoreError()
      {
          window.Error = originalError;
          return { result: "PASS" };
      }

      function testOverriddenToString(obj, overrideThrow)
      {
          if (overrideThrow)
              var func = function() { throw new Error; }
          else
              var func = function() { return [1]; }

          Object.defineProperty(obj, "toString", { value: func, enumerable: false });
          Object.defineProperty(obj, "valueOf", { value: func, enumerable: false });

          // Now the ("" + obj) expression should throw.
          try {
              var dummy = "" + obj;
              console.error("FAIL: Expected to throw but got: " + dummy);
          } catch (e) {
          }

          return obj;
      }
  `);

  TestRunner.runTestSuite([
    function evaluateInConsole(next) {
      var expressions = [
        'testOverriddenArrayPushAndMathMax()',
        'testOverriddenConstructorName()',
        'testThrowConstructorName()',
        'testOverriddenIsFinite()',
        'testOverriddenError()',
        'restoreError()',
        'testOverriddenToString(function func() {}, true)',
        'testOverriddenToString(function func() {}, false)',
        'testOverriddenToString(new Function, true)',
        'testOverriddenToString(new Function, false)',
        'testOverriddenToString(/^regex$/, true)',
        'testOverriddenToString(/^regex$/, false)',
        'testOverriddenToString({}, true)',
        'testOverriddenToString({}, false)',
        'testOverriddenToString(new Number(1), true)',
        'testOverriddenToString(new Number(1), false)',
      ];

      function iterate() {
        var expr = expressions.shift();
        if (!expr) {
          TestRunner.deprecatedRunAfterPendingDispatches(next);
          return;
        }
        ConsoleTestRunner.evaluateInConsole(expr, iterate);
      }
      iterate();
    },

    async function testRuntimeAgentCallFunctionOn(next) {
      var result = await TestRunner.RuntimeAgent.evaluate('({ a : 1, b : 2 })');

      function sum() {
        return this.a + this.b;
      }
      result = await TestRunner.RuntimeAgent.callFunctionOn(sum.toString(), result.objectId);

      TestRunner.assertEquals(3, result.value);
      next();
    },

    async function dumpConsoleMessages(next) {
      await ConsoleTestRunner.dumpConsoleMessages();
      next();
    }
  ]);
})();
