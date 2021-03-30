// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests provisional breakpoints on navigation.');
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await TestRunner.navigate(TestRunner.url('resources/a.html'));

  await SourcesTestRunner.startDebuggerTestPromise();
  let sourceFrame = await SourcesTestRunner.showScriptSourcePromise('a.html');
  TestRunner.addResult('Set breakpoint in inline script and dump it');
  SourcesTestRunner.toggleBreakpoint(sourceFrame, 3, false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
  SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

  TestRunner.addResult('Navigate to the same page and dump stack on pause');
  TestRunner.navigate(TestRunner.url('resources/a.html'));
  await SourcesTestRunner.captureStackTrace(await SourcesTestRunner.waitUntilPausedPromise());
  await new Promise(resolve => SourcesTestRunner.resumeExecution(resolve));

  TestRunner.addResult('Remove breakpoint, set another in not inline script and dump it');
  sourceFrame = await SourcesTestRunner.showScriptSourcePromise('a.html');
  SourcesTestRunner.toggleBreakpoint(sourceFrame, 3, false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);

  sourceFrame = await SourcesTestRunner.showScriptSourcePromise('a.js');
  SourcesTestRunner.toggleBreakpoint(sourceFrame, 9, false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
  SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

  TestRunner.addResult('Navigate to the same page and dump stack on pause');
  TestRunner.navigate(TestRunner.url('resources/a.html'));
  await SourcesTestRunner.captureStackTrace(await SourcesTestRunner.waitUntilPausedPromise());
  await new Promise(resolve => SourcesTestRunner.resumeExecution(resolve));

  SourcesTestRunner.completeDebuggerTest();
})();
