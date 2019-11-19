// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

(async function() {
  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  TestRunner.addResult('Testing accessibility in the call stack sidebar pane.');
  await TestRunner.evaluateInPagePromise(`
      function callWithAsyncStack(f, depth) {
        if (depth === 0) {
          f();
          return;
        }
        wrapper = eval('(function call' + depth + '() { callWithAsyncStack(f, depth - 1) }) //# sourceURL=wrapper.js');
        Promise.resolve().then(wrapper);
      }
      function testFunction() {
        callWithAsyncStack(() => {debugger}, 5);
      }
      //# sourceURL=test.js
    `);

  await SourcesTestRunner.startDebuggerTestPromise(/* quiet */ true);
  await SourcesTestRunner.runTestFunctionAndWaitUntilPausedPromise();
  await TestRunner.addSnifferPromise(
      Sources.CallStackSidebarPane.prototype, '_updatedForTest');

  const callStackPane = runtime.sharedInstance(Sources.CallStackSidebarPane);
  const callStackElement = callStackPane.contentElement;
  TestRunner.addResult(`Call stack pane content: ${TestRunner.clearSpecificInfoFromStackFrames(callStackElement.deepTextContent())}`);
  TestRunner.addResult('Running the axe-core linter on the call stack sidebar pane.');
  await AxeCoreTestRunner.runValidation(callStackElement);

  TestRunner.completeTest();
})();
