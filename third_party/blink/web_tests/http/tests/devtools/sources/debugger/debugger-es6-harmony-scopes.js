// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SourcesModule from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(`Tests ES6 harmony scope sections.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <input type="button" onclick="testFunction()" value="Test">
      <script type="module">
      "use strict";

      let globalLet = 41;
      const globalConst = 42;

      window.makeClosure = function(n)
      {
          let makeClosureBlockVar = "block." + n;
          var makeClosureLocalVar = "local." + n;
          {
              let makeClosureDeeperBlockVar = "block.deep." + n;
              var makeClosureDeeperLocalVar = "local.deep." + n;
              return function innerFunction(x)
              {
                  let innerFunctionBlockVar = x + 102;
                  var innerFunctionLocalVar = x + 2;
                  var negInf = -Infinity;
                  var negZero = 1 / negInf;
                  {
                      let block1 = "block {...}";
                      const const1 = 1;
                      try {
                          throw new Error("An exception");
                      } catch (e) {
                          let block2 = "catch(e) {...}";
                          const const2 = 2;
                          e.toString();
                          debugger;
                      }
                  }
                  return n + makeClosureLocalVar + x + innerFunctionLocalVar + innerFunctionBlockVar +
                      makeClosureBlockVar + makeClosureDeeperBlockVar + makeClosureDeeperLocalVar;
              }
          }
      }
      </script>
    `);
  await TestRunner.evaluateInPagePromise(`
      let globalScriptLet = 41;
      const globalScriptConst = 42;

      function testFunction()
      {
          var f = makeClosure("TextParam");
          f(2014);
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
