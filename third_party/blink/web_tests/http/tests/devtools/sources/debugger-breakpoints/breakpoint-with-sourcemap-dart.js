// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function test() {
  TestRunner.addResult('Checks breakpoint in file with dart sourcemap');
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/breakpoint.js');
  let sourceFrame = await new Promise(
      resolve =>
          SourcesTestRunner.showScriptSource('breakpoint.dart', resolve));
  SourcesTestRunner.toggleBreakpoint(sourceFrame, 2, false);
  SourcesTestRunner.toggleBreakpoint(sourceFrame, 3, false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
  await SourcesTestRunner.waitUntilDebuggerPluginLoaded(sourceFrame);
  SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);
  SourcesTestRunner.completeDebuggerTest();
})();