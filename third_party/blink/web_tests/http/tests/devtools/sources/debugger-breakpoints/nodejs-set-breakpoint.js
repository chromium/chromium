// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {SDKTestRunner} from 'sdk_test_runner';

import * as Host from 'devtools/core/host/host.js';
import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Breakpoints from 'devtools/models/breakpoints/breakpoints.js';

(async function() {
  TestRunner.addResult(`Verify that front-end is able to set breakpoint for node.js scripts.\n`);
  await TestRunner.showPanel('sources');

  const target = SDK.TargetManager.TargetManager.instance().primaryPageTarget()
  target.markAsNodeJSForTest();
  SourcesTestRunner.startDebuggerTest();

  var debuggerModel = target.model(SDK.DebuggerModel.DebuggerModel);
  var functionText = 'function foobar() { \nconsole.log(\'foobar execute!\');\n}';
  var sourceURL = Host.Platform.isWin() ? '\n//# sourceURL=c:\\prog\\foobar.js' : '\n//# sourceURL=/usr/local/home/prog/foobar.js';
  await TestRunner.evaluateInPageAnonymously(functionText + sourceURL);
  SourcesTestRunner.showScriptSource('foobar.js', didShowScriptSource);

  async function didShowScriptSource(sourceFrame) {
    TestRunner.addResult('Setting breakpoint:');
    TestRunner.addSniffer(
        Breakpoints.BreakpointManager.ModelBreakpoint.prototype, 'addResolvedLocation', breakpointResolved);
    await SourcesTestRunner.setBreakpoint(sourceFrame, 1, '', true);
  }

  function breakpointResolved(location) {
    SourcesTestRunner.waitUntilPaused(paused);
    TestRunner.evaluateInPage('foobar()');
  }

  function paused() {
    TestRunner.addResult('Successfully paused on breakpoint');
    SourcesTestRunner.completeDebuggerTest();
  }
})();
