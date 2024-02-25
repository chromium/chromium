// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';
import * as Breakpoints from 'devtools/models/breakpoints/breakpoints.js';

(async function() {
  TestRunner.addResult(
      `Tests that there is no exception in front-end on page reload when breakpoint is set in HTML document and some dynamic scripts are loaded before the script with the breakpoint is loaded.`);
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise(
      'resources/dynamic-scripts-breakpoints.html');

  Breakpoints.BreakpointManager.BreakpointManager.instance().storage.breakpoints = new Map();
  var panel = Sources.SourcesPanel.SourcesPanel.instance();

  SourcesTestRunner.startDebuggerTest();

  SourcesTestRunner.showScriptSource(
      'dynamic-scripts-breakpoints.html', didShowScriptSource);

  function pathToFileName(path) {
    return path.substring(path.lastIndexOf('/') + 1);
  }

  function dumpBreakpointStorage() {
    var breakpointManager = Breakpoints.BreakpointManager.BreakpointManager.instance();
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
        Breakpoints.BreakpointManager.ModelBreakpoint.prototype,
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
