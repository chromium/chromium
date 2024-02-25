// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SourcesModule from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(`Tests "Show more" button in CallStackSidebarPane.`);
  await TestRunner.showPanel('sources');
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
        callWithAsyncStack(() => {debugger}, 36);
      }
      //# sourceURL=test.js
    `);

  await SourcesTestRunner.startDebuggerTestPromise(/* quiet */ true);
  await SourcesTestRunner.runTestFunctionAndWaitUntilPausedPromise();
  await TestRunner.addSnifferPromise(
      SourcesModule.CallStackSidebarPane.CallStackSidebarPane.prototype, 'updatedForTest');
  dumpCallStackSidebarPane();

  TestRunner.addResult('\n---------------\nClicks show more..');
  const pane = SourcesModule.CallStackSidebarPane.CallStackSidebarPane.instance();
  pane.contentElement.querySelector('.show-more-message > .link').click();
  await TestRunner.addSnifferPromise(
      SourcesModule.CallStackSidebarPane.CallStackSidebarPane.prototype, 'updatedForTest');
  dumpCallStackSidebarPane();
  SourcesTestRunner.completeDebuggerTest();

  function dumpCallStackSidebarPane() {
    const pane = SourcesModule.CallStackSidebarPane.CallStackSidebarPane.instance();
    for (const element of pane.contentElement.querySelectorAll(
             '.call-frame-item'))
      TestRunner.addResult(element.deepTextContent().replace(/VM\d+/g, 'VM'));
  }
})();
