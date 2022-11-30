// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Checks that BreakpointManager.possibleBreakpoints returns correct locations\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPageAnonymously(`
      function foo() {
        Promise.resolve().then(() => 239).then(() => 42);
        Promise.resolve();
        return;
      }
      //# sourceURL=foo.js
    `);

  SourcesTestRunner.startDebuggerTestPromise().then(
      () => SourcesTestRunner.showScriptSource('foo.js', didShowScriptSource));

  function didShowScriptSource(sourceFrame) {
    var uiSourceCode = sourceFrame.uiSourceCode();
    var breakpointManager = Bindings.breakpointManager;

    TestRunner.addResult('Locations for first line');
    breakpointManager.possibleBreakpoints(uiSourceCode, new TextUtils.TextRange(0, 0, 1, 0))
        .then(dumpLocations)
        .then(() => TestRunner.addResult('All locations'))
        .then(() => breakpointManager.possibleBreakpoints(uiSourceCode, new TextUtils.TextRange(0, 0, 6, 0)))
        .then(dumpLocations)
        .then(() => TestRunner.addResult('Existing location by position'))
        .then(() => breakpointManager.possibleBreakpoints(uiSourceCode, new TextUtils.TextRange(2, 37, 2, 38)))
        .then(dumpLocations)
        .then(() => TestRunner.addResult('Not existing location by position'))
        .then(() => breakpointManager.possibleBreakpoints(uiSourceCode, new TextUtils.TextRange(2, 38, 2, 39)))
        .then(dumpLocations)
        .then(() => SourcesTestRunner.completeDebuggerTest());
  }

  function dumpLocations(locations) {
    for (var location of locations)
      TestRunner.addResult(`location(${location.lineNumber}, ${location.columnNumber})`);
  }
})();
