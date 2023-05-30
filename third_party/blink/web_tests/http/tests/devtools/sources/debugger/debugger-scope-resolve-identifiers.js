// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests resolving variable names via source maps.\n`);
  await TestRunner.loadLegacyModule('sources');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/resolve-identifiers.js');

  SourcesTestRunner.waitForScriptSource('resolve-identifiers.ts', onSourceMapLoaded);

  function onSourceMapLoaded() {
    SourcesTestRunner.startDebuggerTest(() => SourcesTestRunner.runTestFunctionAndWaitUntilPaused());
    TestRunner.addSniffer(Sources.SourceMapNamesResolver, '_scopeResolvedForTest', onAllScopesResolved, true);
  }

  function onAllScopesResolved() {
    SourcesTestRunner.expandScopeVariablesSidebarPane(onSidebarsExpanded);
  }

  function onSidebarsExpanded() {
    TestRunner.addResult('');
    SourcesTestRunner.dumpScopeVariablesSidebarPane();
    SourcesTestRunner.completeDebuggerTest();
  }
})();
