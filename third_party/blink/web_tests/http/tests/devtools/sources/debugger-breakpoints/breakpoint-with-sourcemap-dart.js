// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function test() {
  TestRunner.addResult('Checks breakpoint in file with dart sourcemap');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/breakpoint.js');
  let sourceFrame = await new Promise(
    resolve =>
    SourcesTestRunner.showScriptSource('breakpoint.dart', resolve));
  await SourcesTestRunner.waitUntilDebuggerPluginLoaded(sourceFrame);
  await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(sourceFrame, [[2, 1], [3, 1]], async () => {
    await SourcesTestRunner.toggleBreakpoint(sourceFrame, 2, false);
    await SourcesTestRunner.toggleBreakpoint(sourceFrame, 3, false);
  });
  SourcesTestRunner.completeDebuggerTest();
})();