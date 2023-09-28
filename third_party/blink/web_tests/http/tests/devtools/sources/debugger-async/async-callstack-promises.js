// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests asynchronous call stacks for Promises.\n`);
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
          var functions = [doTestPromiseConstructor, doTestSettledPromises, doTestChainedPromises, doTestPromiseAll, doTestThrowFromChain, doTestPromiseResolveAndReject];
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

      function doTestPromiseConstructor()
      {
          new Promise(function promiseCallback(resolve, reject) {
              resolve(1);
              debugger;
          });
      }

      function doTestSettledPromises()
      {
          settledPromise("resolved").then(thenCallback, errorCallback);
          settledPromise(Error("rejected")).then(thenCallback, errorCallback);
      }

      function doTestChainedPromises()
      {
          timeoutPromise(1).then(function chained1() {
              debugger;
              return timeoutPromise(2);
          }).then(function chained2() {
              debugger;
              return 3;
          }).then(function chained3() {
              debugger;
              return settledPromise(4);
          }).then(function chained4() {
              debugger;
              return timeoutPromise(5);
          }).then(thenCallback, errorCallback);

          timeoutPromise(1)
              .then(JSON.stringify)
              .then(JSON.parse)
              .then(function afterJSONStringifyAndParse() {
                  debugger;
              });
      }

      function doTestPromiseAll()
      {
          Promise.all([11, 22, 33, 44, 55].map(timeoutPromise))
              .then(thenCallback, errorCallback);
      }

      function doTestThrowFromChain()
      {
          timeoutPromise(1).then(function chained1() {
              return timeoutPromise(2);
          }).then(function chained2() {
              return settledPromise(3);
          }).then(function chained3() {
              throw Error("thrown from chained3");
          }).then(function chained4() {
              return timeoutPromise(5);
          }).catch(function catchCallback() {
              debugger;
          });

          timeoutPromise(1).then(function chained1() {
              return timeoutPromise(2);
          }).then(function chained2() {
              return timeoutPromise(3);
          }).then(function chained3() {
              return timeoutPromise(Error(4));
          }).then(function chained4() {
              return timeoutPromise(5);
          }).catch(function catchCallback() {
              debugger;
          });
      }

      function doTestPromiseResolveAndReject()
      {
          Promise.resolve(1).then(thenCallback, errorCallback);
          Promise.reject(Error("2")).then(thenCallback, errorCallback);
      }
  `);

  var totalDebuggerStatements = 14;
  SourcesTestRunner.runAsyncCallStacksTest(totalDebuggerStatements);
})();
