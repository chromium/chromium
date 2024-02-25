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
      var functions = [doTestChainedPromises, doTestChainedPromisesJSON];
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

    async function doTestChainedPromises()
    {
      try {
        await timeoutPromise(1);
        debugger;
        await timeoutPromise(2);
        debugger;
        await 3;
        debugger;
        await settledPromise(4);
        debugger;
        thenCallback(await timeoutPromise(5));
      } catch (e) {
        errorCallback(e);
      }
    }

    async function doTestChainedPromisesJSON()
    {
      try {
        let one = await timeoutPromise(1);
        let stringify = await JSON.stringify(one);
        let parse = await JSON.parse(stringify);
        thenCallback(parse);
      } catch (e) {
        errorCallback(e);
      }
    }
  `);

  var totalDebuggerStatements = 6;
  SourcesTestRunner.runAsyncCallStacksTest(totalDebuggerStatements);
})();
