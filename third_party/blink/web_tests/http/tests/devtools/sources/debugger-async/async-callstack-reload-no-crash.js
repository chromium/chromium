// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests that page reload with async stacks turned on does not crash. Bug 441223.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          setInterval(callback, 0);
          debugger;
      }

      function callback()
      {
          return window._foo;
      }
  `);

  var maxAsyncCallStackDepth = 8;
  SourcesTestRunner.startDebuggerTest(step1, true);

  async function step1() {
    await TestRunner.DebuggerAgent.setAsyncCallStackDepth(maxAsyncCallStackDepth);
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(didPause);
  }

  function didPause() {
    TestRunner.addResult('Reloading the page...');
    TestRunner.reloadPage(afterReload);
  }

  function afterReload() {
    TestRunner.addResult('PASS: Reloaded successfully.');
    SourcesTestRunner.completeDebuggerTest();
  }
})();
