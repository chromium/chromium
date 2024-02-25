// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests asynchronous call stacks for async functions.\n`);

  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
    function timeoutPromise(value, ms)
    {
      return new Promise(function promiseCallback(resolve, reject) {
        function resolvePromise()
        {
          resolve(value);
        }
        function rejectPromise()
        {
          reject(value);
        }
        if (value instanceof Error)
          setTimeout(rejectPromise, ms || 0);
        else
          setTimeout(resolvePromise, ms || 0);
      });
    }

    function settledPromise(value)
    {
      function resolveCallback(resolve, reject)
      {
        resolve(value);
      }
      function rejectCallback(resolve, reject)
      {
        reject(value);
      }
      if (value instanceof Error)
        return new Promise(rejectCallback);
      else
        return new Promise(resolveCallback);
    }

    function testFunction()
    {
      setTimeout(testFunctionTimeout, 0);
    }

    function testFunctionTimeout()
    {
      var functions = [doTestPromiseAll, doTestThrowFromChain, doTestRejectFromChain, doTestPromiseResolve, doTestPromiseReject];
      for (var i = 0; i < functions.length; ++i)
        functions[i]();
    }

    function thenCallback(value)
    {
      debugger;
    }

    function errorCallback(error)
    {
      debugger;
    }

    async function doTestPromiseAll()
    {
      try {
        let all = await Promise.all([11, 22, 33, 44, 55].map(timeoutPromise));
        thenCallback(all);
      } catch (e) {
        errorCallback(e);
      }
    }

    async function throwFromChain()
    {
      await timeoutPromise(1);
      await timeoutPromise(2);
      await settledPromise(3);
      throw Error("thrown from 4");
      await timeoutPromise(5);
      debugger;
    }

    async function doTestThrowFromChain()
    {
      try {
        let result = await throwFromChain();
        thencallback(result);
      } catch (e) {
        errorCallback(e);
      }
    }

    async function rejectFromChain()
    {
      await timeoutPromise(1);
      await timeoutPromise(2);
      await timeoutPromise(3);
      await timeoutPromise(new Error(4));
      throw new Error("thrown from rejectFromChain");
      debugger;
    }

    async function doTestRejectFromChain()
    {
      try {
        let result = await rejectFromChain();
        thenCallback(result);
      } catch (e) {
        errorCallback(e);
      }
    }

    async function doTestPromiseResolve()
    {
      try {
        let result = await Promise.resolve(1);
        thenCallback(result);
      } catch (e) {
        errorCallback(e);
      }
    }

    async function doTestPromiseReject()
    {
      try {
        let result = await Promise.reject(new Error("2"))
        thenCallback(result);
      } catch (e) {
        errorCallback(e);
      }
    }
  `);

  var totalDebuggerStatements = 5;
  SourcesTestRunner.runAsyncCallStacksTest(totalDebuggerStatements);
})();
