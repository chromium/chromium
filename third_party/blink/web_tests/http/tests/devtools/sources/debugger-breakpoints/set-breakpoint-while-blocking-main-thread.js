// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Root from 'devtools/core/root/root.js';

(async function() {
  // This test is testing the old breakpoint sidebar pane. Make sure to
  // turn off the new breakpoint pane experiment.
  Root.Runtime.experiments.setEnabled('breakpointView', false);
  TestRunner.addResult(`Tests setting breakpoint when main thread blocks.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise('resources/blocking-main-thread.html');

  SourcesTestRunner.runDebuggerTestSuite([
    async function testSetBreakpoint(next) {

      // The debugger plugin needs to be retrieved before pausing, otherwise we
      // cannot set a breakpoint on the main thread during pause.
      var mainThreadSource = await SourcesTestRunner.showScriptSourcePromise(
          'blocking-main-thread.html');
      const plugin = SourcesTestRunner.debuggerPlugin(mainThreadSource);

      SourcesTestRunner.showScriptSource(
          'blocking-main-thread.js', didShowWorkerSource);

      async function didShowWorkerSource(sourceFrame) {
        await SourcesTestRunner.createNewBreakpoint(sourceFrame, 12, '', true);
        await SourcesTestRunner.waitBreakpointSidebarPane();
        SourcesTestRunner.dumpBreakpointSidebarPane();
        SourcesTestRunner.waitUntilPaused(paused);
        TestRunner.addResult('Reloading page.');
        TestRunner.reloadPage();
      }

      async function paused() {
        plugin.createNewBreakpoint(10, '', true);
        await SourcesTestRunner.waitBreakpointSidebarPane();
        SourcesTestRunner.dumpBreakpointSidebarPane();
        next();
      }
    }
  ]);
})();
