// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function test() {
  TestRunner.addResult('Checks stepInto fetch');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('sources');

  await SourcesTestRunner.startDebuggerTestPromise();
  ConsoleTestRunner.evaluateInConsole(`
    debug(fetch);
    fetch("../debugger/resources/script1.js");
    //# sourceURL=test.js`);
  await SourcesTestRunner.waitUntilPausedPromise();
  SourcesTestRunner.stepIntoAsync();
  let callFrames = await SourcesTestRunner.waitUntilPausedPromise();
  await SourcesTestRunner.captureStackTrace(callFrames);
  SourcesTestRunner.completeDebuggerTest();
})();