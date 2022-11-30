// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that there is no exception in front-end on page reload when breakpoint is set in HTML document and some dynamic scripts are loaded before the script with the breakpoint is loaded.`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise(
      'resources/dynamic-scripts-breakpoints.html');

  Bindings.breakpointManager.storage.breakpoints = new Map();
  var panel = UI.panels.sources;

  SourcesTestRunner.startDebuggerTest();

  SourcesTestRunner.showScriptSource(
      'dynamic-scripts-breakpoints.html', didShowScriptSource);

  function pathToFileName(path) {
    return path.substring(path.lastIndexOf('/') + 1);
  }

  function dumpBreakpointStorage() {
    var breakpointManager = Bindings.breakpointManager;
    var breakpoints = breakpointManager.storage.setting.get();
    TestRunner.addResult('    Dumping breakpoint storage');
    for (var i = 0; i < breakpoints.length; ++i)
      TestRunner.addResult(
          '        ' + pathToFileName(breakpoints[i].url) + ':' +
          breakpoints[i].lineNumber);
  }

  async function didShowScriptSource(sourceFrame) {
    TestRunner.addResult('Setting breakpoint:');
    TestRunner.addSniffer(
        Bindings.BreakpointManager.ModelBreakpoint.prototype,
        'addResolvedLocation', breakpointResolved);
    await SourcesTestRunner.setBreakpoint(sourceFrame, 7, '', true);
  }

  function breakpointResolved(location) {
    SourcesTestRunner.waitUntilPaused(paused);
    TestRunner.addResult('Reloading page.');
    TestRunner.reloadPage(onPageReloaded);
  }

  function paused() {
    dumpBreakpointStorage();
    SourcesTestRunner.resumeExecution();
  }

  function onPageReloaded() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
