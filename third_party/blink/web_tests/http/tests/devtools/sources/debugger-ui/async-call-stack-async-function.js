// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SourcesModule from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(`Tests that call stack sidebar contains correct labels for async await functions.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      async function foo()
      {
          await Promise.resolve(1);
          await Promise.resolve(2);
          debugger;
      }

      async function boo()
      {
          await Promise.resolve(3);
          await foo();
      }

      async function testFunction()
      {
          await Promise.resolve(4);
          await boo();
      }
      //# sourceURL=test.js
    `);

  TestRunner.DebuggerAgent.setAsyncCallStackDepth(200);

  SourcesTestRunner.startDebuggerTestPromise(/* quiet */ true)
      .then(() => SourcesTestRunner.runTestFunctionAndWaitUntilPausedPromise())
      .then(
          () => TestRunner.addSnifferPromise(
              SourcesModule.CallStackSidebarPane.CallStackSidebarPane.prototype, 'updatedForTest'))
      .then(() => dumpCallStackSidebarPane())
      .then(() => SourcesTestRunner.completeDebuggerTest());

  function dumpCallStackSidebarPane() {
    var pane = SourcesModule.CallStackSidebarPane.CallStackSidebarPane.instance();
    for (var element of pane.contentElement.querySelectorAll('.call-frame-item'))
      TestRunner.addResult(element.deepTextContent().replace(/VM\d+/g, 'VM'));
  }
})();
