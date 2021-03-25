// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function test() {
  TestRunner.addResult('Checks inline breakpoint in file with sourcemap');
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await SourcesTestRunner.startDebuggerTestPromise();
  /* arrow.ts, js generated with tsc arrow.ts --inlineSourceMap --inlineSources
function foo() {
  function create() {
    return (a: number) => (b: number) => (c: number) => (d: number) => a + b + c + d;
  }
  create()(1)(2)(3);
}
  */
  await TestRunner.evaluateInPageAnonymously(`function foo() {
    function create() {
        return function (a) { return function (b) { return function (c) { return function (d) { return a + b + c + d; }; }; }; };
    }
    create()(1)(2)(3);
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiYXJyb3cuanMiLCJzb3VyY2VSb290IjoiIiwic291cmNlcyI6WyJhcnJvdy50cyJdLCJuYW1lcyI6WyJmb28iLCJmb28uY3JlYXRlIl0sIm1hcHBpbmdzIjoiQUFBQTtJQUNFQTtRQUNFQyxNQUFNQSxDQUFDQSxVQUFDQSxDQUFTQSxJQUFLQSxPQUFBQSxVQUFDQSxDQUFTQSxJQUFLQSxPQUFBQSxVQUFDQSxDQUFTQSxJQUFLQSxPQUFBQSxVQUFDQSxDQUFTQSxJQUFLQSxPQUFBQSxDQUFDQSxHQUFHQSxDQUFDQSxHQUFHQSxDQUFDQSxHQUFHQSxDQUFDQSxFQUFiQSxDQUFhQSxFQUE1QkEsQ0FBNEJBLEVBQTNDQSxDQUEyQ0EsRUFBMURBLENBQTBEQSxDQUFDQTtJQUNuRkEsQ0FBQ0E7SUFDREQsTUFBTUEsRUFBRUEsQ0FBQ0EsQ0FBQ0EsQ0FBQ0EsQ0FBQ0EsQ0FBQ0EsQ0FBQ0EsQ0FBQ0EsQ0FBQ0EsQ0FBQ0EsQ0FBQ0E7QUFDcEJBLENBQUNBIiwic291cmNlc0NvbnRlbnQiOlsiZnVuY3Rpb24gZm9vKCkge1xuICBmdW5jdGlvbiBjcmVhdGUoKSB7XG4gICAgcmV0dXJuIChhOiBudW1iZXIpID0+IChiOiBudW1iZXIpID0+IChjOiBudW1iZXIpID0+IChkOiBudW1iZXIpID0+IGEgKyBiICsgYyArIGQ7XG4gIH1cbiAgY3JlYXRlKCkoMSkoMikoMyk7XG59XG4iXX0=`);

  let sourceFrame = await new Promise(resolve => SourcesTestRunner.showScriptSource('arrow.ts', resolve));
  TestRunner.addResult('Setting breakpoint at second line..');
  // We expect 7 breakpoint decorations on line 2.
  await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(sourceFrame, [[2, 7]], () =>
    SourcesTestRunner.toggleBreakpoint(sourceFrame, 2, false));

  TestRunner.addResult('Disabling first breakpoint and enable forth breakpoint at line..');
  await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(sourceFrame, [[2, 7]], () => {
    SourcesTestRunner.clickDebuggerPluginBreakpoint(sourceFrame, 2, 0);
    SourcesTestRunner.clickDebuggerPluginBreakpoint(sourceFrame, 2, 3);
  });

  TestRunner.addResult('Calling function foo..');
  TestRunner.evaluateInPageAnonymously('foo()');
  let callFrames = await SourcesTestRunner.waitUntilPausedPromise();
  let pausedLocation = await Bindings.debuggerWorkspaceBinding.rawLocationToUILocation(callFrames[0].location());
  TestRunner.addResult(`Paused at: (${pausedLocation.lineNumber}, ${pausedLocation.columnNumber})`);
  await SourcesTestRunner.captureStackTrace(callFrames);
  SourcesTestRunner.completeDebuggerTest();
})();