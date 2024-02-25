// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SourcesModule from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(
      `Tests that evaluation in console works fine when script is paused. It also checks that stack and global variables are accessible from the console.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      var globalVar = { b: 1 };

      function slave(x)
      {
          var y = 20;
          debugger;
      }

      function testFunction()
      {
          var localObject = { a: 300 };
          slave(4000);
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused();
    TestRunner.addSniffer(
              SourcesModule.CallStackSidebarPane.CallStackSidebarPane.prototype, 'updatedForTest', step2);
  }

  function step2(callFrames) {
    ConsoleTestRunner.evaluateInConsole('x + y + globalVar.b', step3.bind(null, callFrames));
  }

  function step3(callFrames, result) {
    TestRunner.addResult('Evaluated script on the top frame: ' + result);
    var pane = SourcesModule.CallStackSidebarPane.CallStackSidebarPane.instance();
    pane.selectNextCallFrameOnStack();
    TestRunner.deprecatedRunAfterPendingDispatches(step4);
  }

  function step4() {
    ConsoleTestRunner.evaluateInConsole('localObject.a + globalVar.b', step5);
  }

  function step5(result) {
    TestRunner.addResult('Evaluated script on the calling frame: ' + result);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
