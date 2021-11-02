// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function test() {
  TestRunner.addResult('Checks breakpoint in file with sourcemap');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await SourcesTestRunner.startDebuggerTestPromise();
  await TestRunner.evaluateInPageAnonymously(`function foo() {
    var a = 1;
    return a;
}
foo();
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiaW5kZXguanMiLCJzb3VyY2VSb290IjoiIiwic291cmNlcyI6WyJpbmRleC50cyJdLCJuYW1lcyI6W10sIm1hcHBpbmdzIjoiQUFBQTtJQUNFLElBQUksQ0FBQyxHQUFHLENBQUMsQ0FBQztJQUVWLE1BQU0sQ0FBQyxDQUFDLENBQUM7QUFDWCxDQUFDO0FBQ0QsR0FBRyxFQUFFLENBQUMiLCJzb3VyY2VzQ29udGVudCI6WyJmdW5jdGlvbiBmb28oKSB7XG4gIHZhciBhID0gMTtcblxuICByZXR1cm4gYTtcbn1cbmZvbygpOyJdfQ==`);

  let sourceFrame = await new Promise(resolve => SourcesTestRunner.showScriptSource('index.ts', resolve));
  SourcesTestRunner.toggleBreakpoint(sourceFrame, 1, false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);
  TestRunner.evaluateInPageAnonymously('foo()');
  let callFrames = await SourcesTestRunner.waitUntilPausedPromise();
  await SourcesTestRunner.captureStackTrace(callFrames);
  SourcesTestRunner.completeDebuggerTest();
})();