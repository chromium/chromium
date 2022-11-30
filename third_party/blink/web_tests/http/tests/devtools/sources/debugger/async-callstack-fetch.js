// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests asynchronous call stacks for fetch.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          setTimeout(doFetch, 0);
      }

      function doFetch()
      {
          fetch("../debugger/resources/script1.js").then(function chained1() {
              debugger;
          }).then(function chained2() {
          }).then(function chained3() {
          }).then(function chained4() {
              debugger;
          });
      }
  `);

  var totalDebuggerStatements = 2;
  SourcesTestRunner.runAsyncCallStacksTest(totalDebuggerStatements);
})();
