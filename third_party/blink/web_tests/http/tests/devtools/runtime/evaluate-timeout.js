// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as UIModule from 'devtools/ui/legacy/legacy.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult("Test frontend's timeout support.\n");

  const executionContext = UIModule.Context.Context.instance().flavor(SDK.RuntimeModel.ExecutionContext);
  const regularExpression = '1 + 1';
  const infiniteExpression = 'while (1){}';

  await runtimeTestCase(infiniteExpression, 0);
  await runtimeTestCase(regularExpression);

  // Debugger evaluateOnCallFrame test.
  await TestRunner.evaluateInPagePromise(`
    function testFunction()
    {
        debugger;
    }
  `);
  await TestRunner.showPanel('sources');

  await SourcesTestRunner.startDebuggerTestPromise();
  await SourcesTestRunner.runTestFunctionAndWaitUntilPausedPromise();

  await debuggerTestCase(infiniteExpression, 0);
  await debuggerTestCase(regularExpression);

  SourcesTestRunner.completeDebuggerTest();

  async function runtimeTestCase(expression, timeout) {
    TestRunner.addResult(`\nTesting expression ${expression} with timeout: ${timeout}`);
    const result = await executionContext.evaluate({expression, timeout});
    printDetails(result);
  }

  async function debuggerTestCase(expression, timeout) {
    TestRunner.addResult(`\nTesting expression ${expression} with timeout: ${timeout}`);
    const result = await executionContext.debuggerModel.selectedCallFrame().evaluate({expression, timeout});
    printDetails(result);
  }

  function printDetails(result) {
    if (result.error) {
      TestRunner.addResult(`Error: ${result.error}`);
    } else {
      TestRunner.addResult('Result:');
      TestRunner.addResult(`  Description: ${result.object.description}`);
      TestRunner.addResult(`  Value:       ${result.object.value}`);
      TestRunner.addResult(`  Type:        ${result.object.type}`);

    }
  }
})();
