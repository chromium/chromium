// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests asynchronous call stacks for setInterval.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      var intervalId;
      var count = 0;

      function testFunction()
      {
          intervalId = setInterval(callback, 0);
      }

      function callback()
      {
          if (count === 0) {
              debugger;
          } else if (count === 1) {
              debugger;
          } else {
              clearInterval(intervalId);
              debugger;
          }
          ++count;
      }
  `);

  var totalDebuggerStatements = 3;
  SourcesTestRunner.runAsyncCallStacksTest(totalDebuggerStatements);
})();
