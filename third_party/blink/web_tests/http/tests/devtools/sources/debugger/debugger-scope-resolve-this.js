// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SourcesModule from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(`Tests resolving this object name via source maps.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/resolve-this.js');

  SourcesTestRunner.waitForScriptSource('resolve-this.ts', onSourceMapLoaded);

  function onSourceMapLoaded() {
    SourcesTestRunner.startDebuggerTest(() => SourcesTestRunner.runTestFunctionAndWaitUntilPaused());
    TestRunner.addSniffer(
        SourcesModule.ScopeChainSidebarPane.ScopeChainSidebarPane.prototype, 'sidebarPaneUpdatedForTest', onSidebarRendered, true);
  }

  function onSidebarRendered() {
    SourcesTestRunner.expandScopeVariablesSidebarPane(onSidebarsExpanded);
  }

  function onSidebarsExpanded() {
    TestRunner.addResult('');
    SourcesTestRunner.dumpScopeVariablesSidebarPane();
    SourcesTestRunner.completeDebuggerTest();
  }
})();
