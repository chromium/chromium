// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that pause on promise rejection works.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function createPromise()
      {
          var result = {};
          var p = new Promise(function(resolve, reject) {
              result.resolve = resolve;
              result.reject = reject;
          });
          result.promise = p;
          return result;
      }

      function testFunction()
      {
          var resolved = createPromise();
          var caught = createPromise();
          var uncaught = createPromise();

          caught.promise
              .then(function c1() {})
              .then(function c2() {})
              .catch(function c3() {});
          uncaught.promise
              .then(function f1() {})
              .then(function f2() {})
              .then(function f3() {}); // Last is uncaught.

          resolved.resolve(42); // Should not pause.
          caught.reject(new Error("caught"));
          uncaught.reject(new Error("uncaught"));
      }
  `);

  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.startDebuggerTest(step1);

  function waitUntilPausedNTimes(count, callback) {
    function inner() {
      if (count--)
        SourcesTestRunner.waitUntilPausedAndDumpStackAndResume(inner);
      else
        callback();
    }
    inner();
  }

  function step1() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(
        SDK.DebuggerModel.PauseOnExceptionsState.PauseOnUncaughtExceptions);
    SourcesTestRunner.showScriptSource(
        'debugger-pause-on-promise-rejection.js', step2);
  }

  function step2() {
    TestRunner.addResult('=== Pausing only on uncaught exceptions ===');
    SourcesTestRunner.runTestFunction();
    waitUntilPausedNTimes(1, step3);
  }

  function step3() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(
        SDK.DebuggerModel.PauseOnExceptionsState.PauseOnAllExceptions);
    TestRunner.addResult('\n=== Pausing on all exceptions ===');
    SourcesTestRunner.runTestFunction();
    waitUntilPausedNTimes(2, step4);
  }

  function step4() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(
        SDK.DebuggerModel.PauseOnExceptionsState.DontPauseOnExceptions);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
