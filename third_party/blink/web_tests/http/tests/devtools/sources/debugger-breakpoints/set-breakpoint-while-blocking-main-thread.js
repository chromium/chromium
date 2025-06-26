// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';
import * as RenderCoordinator from 'devtools/ui/components/render_coordinator/render_coordinator.js';

async function dumpBreakpointSidebarPane() {
  var pane = Sources.BreakpointsView.BreakpointsView.instance();
  await Sources.BreakpointsView.BreakpointsSidebarController.instance().update();
  await RenderCoordinator.done();
  await pane.updateComplete;
  const groupHeader = pane.contentElement.querySelectorAll('[role="group"]');
  for (let i = 0; i < groupHeader?.length; ++i) {
    const title = groupHeader[i].querySelector('.group-header-title')?.textContent;
    const line = groupHeader[i].querySelector('.breakpoint-item .location')?.textContent;
    const code = groupHeader[i].querySelector('.breakpoint-item .code-snippet')?.textContent;
    TestRunner.addResult(`${title}:line ${line}: ${code}`);
  }
}

(async function() {
  TestRunner.addResult(`Tests setting breakpoint when main thread blocks.\n`);
  await TestRunner.showPanel('sources');
  TestRunner.navigate('resources/blocking-main-thread.php');

  SourcesTestRunner.runDebuggerTestSuite([
    async function testSetBreakpoint(next) {
      SourcesTestRunner.showScriptSource(
            'blocking-main-thread-worker.php', didShowWorkerSource);

      async function didShowWorkerSource(sourceFrame) {
        await SourcesTestRunner.setBreakpoint(sourceFrame, 8, '', true);
        await dumpBreakpointSidebarPane();
        SourcesTestRunner.waitUntilPaused(paused);
        TestRunner.reloadPage();
      }

      async function paused() {
        var mainThreadSource = await SourcesTestRunner.showScriptSourcePromise(
          'blocking-main-thread.php');
        await SourcesTestRunner.setBreakpoint(mainThreadSource, 5, '', true);
        await dumpBreakpointSidebarPane();
        next();
      }
    }
  ]);
})();
