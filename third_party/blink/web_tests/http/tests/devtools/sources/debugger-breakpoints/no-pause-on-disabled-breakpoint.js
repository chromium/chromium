// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests disabled breakpoint.');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await TestRunner.addScriptTag('resources/a.js');

  await SourcesTestRunner.startDebuggerTestPromise();
  const sourceFrame = await SourcesTestRunner.showScriptSourcePromise('a.js');
  TestRunner.addResult('Set breakpoint');
  // We expect 1 Breakpoint decoration on line 9.
  await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(sourceFrame, [[9, 1]], () =>
    SourcesTestRunner.toggleBreakpoint(sourceFrame, 9, false));

  TestRunner.addResult('Run function and check pause');
  let pausePromise = SourcesTestRunner.waitUntilPausedPromise();
  TestRunner.evaluateInPage('main()//# sourceURL=test.js');
  await SourcesTestRunner.captureStackTrace(await pausePromise);
  await new Promise(resolve => SourcesTestRunner.resumeExecution(resolve));

  TestRunner.addResult('Disable breakpoint');
  await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(sourceFrame, [[9, 1]], () =>
    SourcesTestRunner.toggleBreakpoint(sourceFrame, 9, true));

  TestRunner.addResult('Run function and check that pause happens after function');
  pausePromise = SourcesTestRunner.waitUntilPausedPromise();
  TestRunner.evaluateInPage('main(); debugger;//# sourceURL=test.js');
  await SourcesTestRunner.captureStackTrace(await pausePromise);
  await new Promise(resolve => SourcesTestRunner.resumeExecution(resolve));

  SourcesTestRunner.completeDebuggerTest();
})();
