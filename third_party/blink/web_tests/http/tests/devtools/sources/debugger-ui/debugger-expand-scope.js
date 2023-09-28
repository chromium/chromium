// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SourcesModule from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(
      `Test that sections representing scopes of the current call frame are expandable and contain correct data.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <input type="button" onclick="testFunction()" value="Test">
    `);
  await TestRunner.evaluateInPagePromise(`
      function makeClosure(n)
      {
          var makeClosureLocalVar = "local." + n;
          return function innerFunction(x)
          {
              var innerFunctionLocalVar = x + 2;
              var negInf = -Infinity;
              var negZero = 1 / negInf;
              try {
                  throw new Error("An exception");
              } catch (e) {
                  e.toString();
                  debugger;
              }
              return n + makeClosureLocalVar + x + innerFunctionLocalVar;
          }
      }

      function testFunction()
      {
          var f = makeClosure("TextParam");
          f(2010);
      }
  `);

  SourcesTestRunner.startDebuggerTest(onTestStarted);

  function onTestStarted() {
    TestRunner.addSniffer(
        SourcesModule.ScopeChainSidebarPane.ScopeChainSidebarPane.prototype, 'sidebarPaneUpdatedForTest', onSidebarRendered, true);
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(() => {});
  }

  function onSidebarRendered() {
    SourcesTestRunner.expandScopeVariablesSidebarPane(onScopeVariablesExpanded);
  }

  function onScopeVariablesExpanded() {
    TestRunner.addResult('');
    SourcesTestRunner.dumpScopeVariablesSidebarPane();
    SourcesTestRunner.completeDebuggerTest();
  }
})();
