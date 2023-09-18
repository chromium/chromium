// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SourcesModule from 'devtools/panels/sources/sources.js';

(async function() {
  await TestRunner.showPanel('sources');

  TestRunner.addResult('Testing accessibility in the call stack sidebar pane.');
  await TestRunner.evaluateInPagePromise(`
      function callWithAsyncStack(f, depth) {
        if (depth === 0) {
          f();
          return;
        }
        wrapper = eval('(function call' + depth + '() { callWithAsyncStack(f, depth - 1) }) //# sourceURL=wrapper.js');
        queueMicrotask(wrapper);
      }
      function testFunction() {
        callWithAsyncStack(() => {debugger}, 5);
      }
      //# sourceURL=test.js
    `);

  await SourcesTestRunner.startDebuggerTestPromise(/* quiet */ true);
  await SourcesTestRunner.runTestFunctionAndWaitUntilPausedPromise();
  await TestRunner.addSnifferPromise(SourcesModule.CallStackSidebarPane.CallStackSidebarPane.prototype, 'updatedForTest');

  const callStackPane = SourcesModule.CallStackSidebarPane.CallStackSidebarPane.instance();
  const callStackElement = callStackPane.contentElement;
  TestRunner.addResult(`Call stack pane content: ${TestRunner.clearSpecificInfoFromStackFrames(callStackElement.deepTextContent())}`);
  TestRunner.addResult('Running the axe-core linter on the call stack sidebar pane.');
  await AxeCoreTestRunner.runValidation(callStackElement);

  TestRunner.completeTest();
})();
