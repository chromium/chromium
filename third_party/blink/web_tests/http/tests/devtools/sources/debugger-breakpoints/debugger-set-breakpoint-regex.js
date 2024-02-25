// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests Debugger.setBreakpointByUrl with isRegex set to true.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          foo();
      }

      function foo()
      {
      }
  `);

  SourcesTestRunner.runDebuggerTestSuite([
    async function testSetNoneOfURLAndRegex(next) {
      var response = await TestRunner.DebuggerAgent.invoke_setBreakpointByUrl({lineNumber: 1});
      TestRunner.addResult(response.getError());
      next();
    },

    async function testSetBothURLAndRegex(next) {
      var url = 'debugger-set-breakpoint.js';
      var urlRegex = 'debugger-set-breakpoint.*';
      var response = await TestRunner.DebuggerAgent.invoke_setBreakpointByUrl({lineNumber: 1, url, urlRegex});
      TestRunner.addResult(response.getError());
      next();
    },

    async function testSetByRegex(next) {
      await TestRunner.DebuggerAgent.invoke_setBreakpointByUrl({urlRegex: 'debugger-set-breakpoint.*', lineNumber: 11});
      SourcesTestRunner.runTestFunctionAndWaitUntilPaused(async callFrames => {
        await SourcesTestRunner.captureStackTrace(callFrames);
        next();
      });
    }
  ]);
})();
