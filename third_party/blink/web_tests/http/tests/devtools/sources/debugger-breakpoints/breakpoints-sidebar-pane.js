// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  // This test is testing the old breakpoint sidebar pane. Make sure to
  // turn off the new breakpoint pane experiment.
  Root.Runtime.experiments.setEnabled('breakpointView', false);
  TestRunner.addResult(`Tests that breakpoints appear correct in the sidebar pane.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise('resources/a.html');

  const sourceFrame = await SourcesTestRunner.showScriptSourcePromise(
      'a.js');
  TestRunner.addResult('Script source was shown.');

  const debuggerPlugin = SourcesTestRunner.debuggerPlugin(sourceFrame);

  TestRunner.addResult('\nSet first breakpoint.');
  debuggerPlugin.setBreakpoint(17, 2, '', true);
  await SourcesTestRunner.waitBreakpointSidebarPane();
  SourcesTestRunner.dumpBreakpointSidebarPane();

  TestRunner.addResult('\nSet second breakpoint on the same line.');
  debuggerPlugin.setBreakpoint(17, 15, '', true);
  await SourcesTestRunner.waitBreakpointSidebarPane();
  SourcesTestRunner.dumpBreakpointSidebarPane();

  TestRunner.addResult('\nSet a third breakpoint on a different line.');
  debuggerPlugin.setBreakpoint(16, 2, '', true);
  await SourcesTestRunner.waitBreakpointSidebarPane();
  SourcesTestRunner.dumpBreakpointSidebarPane();

  TestRunner.completeTest();
})();
