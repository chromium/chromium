// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that front-end is able to set breakpoint for node.js scripts.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadTestModule('sdk_test_runner');
  await TestRunner.showPanel('sources');

  SDK.targetManager.mainTarget().markAsNodeJSForTest();
  SourcesTestRunner.startDebuggerTest();

  var debuggerModel = SDK.targetManager.mainTarget().model(SDK.DebuggerModel);
  var functionText = 'function foobar() { \nconsole.log(\'foobar execute!\');\n}';
  var sourceURL = Host.isWin() ? '\n//# sourceURL=c:\\prog\\foobar.js' : '\n//# sourceURL=/usr/local/home/prog/foobar.js';
  await TestRunner.evaluateInPageAnonymously(functionText + sourceURL);
  SourcesTestRunner.showScriptSource('foobar.js', didShowScriptSource);

  async function didShowScriptSource(sourceFrame) {
    TestRunner.addResult('Setting breakpoint:');
    TestRunner.addSniffer(
        Bindings.BreakpointManager.ModelBreakpoint.prototype, 'addResolvedLocation', breakpointResolved);
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
