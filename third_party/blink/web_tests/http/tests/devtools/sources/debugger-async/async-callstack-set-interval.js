// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests asynchronous call stacks for setInterval.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
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
