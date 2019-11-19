// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Checks that locations are correctly resolved for gutter click.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPageAnonymously(`
      function foo() {
        var p = Promise.resolve()
          .then(() => 239);
        console.log(42);
        // comments 1
        // comments 2
        // comments 3
        // comments 4
        // comments 5
        fetch("url").then(response => response.data()).then(data => data.json());
        // comment 6
        // super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment, super long comment,
        // comment 7
        Promise.resolve();
        return p;
      }
      //# sourceURL=foo.js
    `);

  SourcesTestRunner.startDebuggerTestPromise().then(
      () => SourcesTestRunner.showScriptSource('foo.js', didShowScriptSource));

  async function didShowScriptSource(sourceFrame) {
    await SourcesTestRunner.waitUntilDebuggerPluginLoaded(sourceFrame);
    var uiSourceCode = sourceFrame._uiSourceCode;
    var breakpointManager = Bindings.breakpointManager;
    setBreakpoint(breakpointManager, sourceFrame, 3, false)
        .then(() => setBreakpoint(breakpointManager, sourceFrame, 4, false))
        .then(() => setBreakpoint(breakpointManager, sourceFrame, 5, false))
        .then(() => setBreakpoint(breakpointManager, sourceFrame, 6, false))
        .then(() => setBreakpoint(breakpointManager, sourceFrame, 11, false))
        .then(() => setBreakpoint(breakpointManager, sourceFrame, 12, false))
        .then(() => setBreakpoint(breakpointManager, sourceFrame, 13, false))
        .then(() => SourcesTestRunner.completeDebuggerTest());
  }

  function setBreakpoint(
      breakpointManager, sourceFrame, lineNumberClicked, shiftKey) {
    var resolveCallback;
    var promise = new Promise(resolve => resolveCallback = resolve);
    TestRunner.addSniffer(
        Sources.DebuggerPlugin.prototype, '_breakpointWasSetForTest',
        dumpLocation, false);
    SourcesTestRunner.debuggerPlugin(sourceFrame)._handleGutterClick({
      data: {
        lineNumber: lineNumberClicked,
        event: {button: 0, shiftKey: shiftKey, consume: () => true},
        gutterType: SourceFrame.SourcesTextEditor.lineNumbersGutterType
      }
    });
    return promise;

    function dumpLocation(lineNumber, columnNumber, condition, enabled) {
      TestRunner.addResult(
          `${lineNumberClicked}: breakpointAdded(${lineNumber}, ${
                                                                  columnNumber
                                                                })`);
      resolveCallback();
    }
  }
})();
