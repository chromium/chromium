// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that breakpoints work in anonymous scripts with >1 targets.\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          var workerScript = \`
          eval('function foo() {}');
          postMessage('ready')\`;
          var blob = new Blob([workerScript], { type: "text/javascript" });
          worker = new Worker(URL.createObjectURL(blob));
          worker.onmessage = () => {
              eval("debugger;" + "\\n".repeat(10) + "function foo() { }");
          }
      }
  `);

  await SourcesTestRunner.startDebuggerTestPromise();
  TestRunner.evaluateInPageWithTimeout('testFunction()');
  var sourceFrame = await waitForPausedUISourceCode();
  await SourcesTestRunner.createNewBreakpoint(sourceFrame, 10, '', true);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
  await SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);
  SourcesTestRunner.completeDebuggerTest();

  function waitForPausedUISourceCode() {
    return new Promise(resolve => {
      TestRunner.addSniffer(
          Sources.DebuggerPlugin.prototype, '_executionLineChanged',
          function() {
            resolve(UI.panels.sources.visibleView);
          });
    });
  }
})();
