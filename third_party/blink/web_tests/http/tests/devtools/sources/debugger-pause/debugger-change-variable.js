// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SourcesModule from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(`Tests that modifying local variables works fine.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function slave(x)
      {
          var y = 20;
          debugger;
      }

      function testFunction()
      {
          var localObject1 = { a: 310 };
          var localObject2 = 42;
          slave(4000);
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function evalLocalVariables(callback) {
    ConsoleTestRunner.evaluateInConsoleAndDump('localObject1.a', next);
    function next() {
      ConsoleTestRunner.evaluateInConsoleAndDump('localObject2', callback);
    }
  }

  function localScopeObject() {
    var localsSection = SourcesTestRunner.scopeChainSections()[0];
    return localsSection.object;
  }

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused();
    TestRunner.addSniffer(
              SourcesModule.CallStackSidebarPane.CallStackSidebarPane.prototype, 'updatedForTest', step2);
  }

  function step2(callFrames) {
    var pane = SourcesModule.CallStackSidebarPane.CallStackSidebarPane.instance();
    pane.selectNextCallFrameOnStack();
    TestRunner.deprecatedRunAfterPendingDispatches(step3);
  }

  function step3() {
    TestRunner.addResult('\nEvaluated before modification:');
    evalLocalVariables(step4);
  }

  async function step4() {
    await localScopeObject().setPropertyValue('localObject1', '({ a: -290})');
    await localScopeObject().setPropertyValue({value: 'localObject2'}, '123');
    TestRunner.addResult('\nEvaluated after modification:');
    evalLocalVariables(step7);
  }

  function step7() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
