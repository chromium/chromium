// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as UIModule from 'devtools/ui/legacy/legacy.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult("Test frontend's side-effect support check for compatibility.\n");

  const executionContext = UIModule.Context.Context.instance().flavor(SDK.RuntimeModel.ExecutionContext);
  const expressionWithSideEffect = '(async function(){ await 1; })()';
  const expressionWithoutSideEffect = '1 + 1';

  await runtimeTestCase(expressionWithSideEffect, /* throwOnSideEffect */ true);
  await runtimeTestCase(expressionWithSideEffect, /* throwOnSideEffect */ false);
  await runtimeTestCase(expressionWithoutSideEffect, /* throwOnSideEffect */ true);
  await runtimeTestCase(expressionWithoutSideEffect, /* throwOnSideEffect */ false);

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

  await debuggerTestCase(expressionWithSideEffect, /* throwOnSideEffect */ true);
  await debuggerTestCase(expressionWithSideEffect, /* throwOnSideEffect */ false);
  await debuggerTestCase(expressionWithoutSideEffect, /* throwOnSideEffect */ true);
  await debuggerTestCase(expressionWithoutSideEffect, /* throwOnSideEffect */ false);

  SourcesTestRunner.completeDebuggerTest();

  async function runtimeTestCase(expression, throwOnSideEffect) {
    TestRunner.addResult(`\nTesting expression ${expression} with throwOnSideEffect ${throwOnSideEffect}`);
    const result = await executionContext.evaluate({expression, throwOnSideEffect});
    printDetails(result);
  }

  async function debuggerTestCase(expression, throwOnSideEffect) {
    TestRunner.addResult(`\nTesting expression ${expression} with throwOnSideEffect ${throwOnSideEffect}`);
    const result = await executionContext.debuggerModel.selectedCallFrame().evaluate({expression, throwOnSideEffect});
    printDetails(result);
  }

  async function printDetails(result) {
    if (result.error) {
      TestRunner.addResult(`FAIL - Error: ${result.error}`);
    } else if (result.exceptionDetails) {
      let exceptionDescription = result.exceptionDetails.exception.description;
      TestRunner.addResult(`Exception: ${exceptionDescription.split("\n")[0]}`);
    } else if (result.object) {
      let objectDescription = result.object.description;
      TestRunner.addResult(`Result: ${objectDescription}`);
    }
  }
})();
