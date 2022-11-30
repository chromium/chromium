// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that callFrames on pause contains function name taking into account Function.name (and ignoring displayName).\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      var foo = function ()
      {
          debugger;
      }

      foo.displayName = 'foo.displayName';
      Object.defineProperty(foo, 'name', { value: 'foo.function.name' } );

      var bar = function()
      {
          foo();
      }

      bar.displayName = 'bar.displayName';

      var baz = function()
      {
          bar();
      }

      Object.defineProperty(baz, 'name', { value: 'baz.function.name' } );

      function testFunction()
      {
          baz();
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2(callFrames) {
    TestRunner.addResult('callFrames.length = ' + callFrames.length);
    for (var i = 0; i < callFrames.length; ++i)
      TestRunner.addResult('functionName: ' + callFrames[i].functionName);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
