// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as UIModule from 'devtools/ui/legacy/legacy.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests separate compilation and run.\n`);
  await TestRunner.showPanel('sources');

  function printExceptionDetails(exceptionDetails) {
    TestRunner.addResult('exceptionDetails:');
    var text = exceptionDetails.text;
    if (exceptionDetails.exception)
      text += ' ' + exceptionDetails.exception.description;
    TestRunner.addResult('   ' + text);
    TestRunner.addResult('   line: ' + exceptionDetails.lineNumber + ', column: ' + exceptionDetails.columnNumber);

    var stack = exceptionDetails.stackTrace ? exceptionDetails.stackTrace.callFrames : null;
    if (!stack) {
      TestRunner.addResult('   no stack trace attached to exceptionDetails');
    } else {
      TestRunner.addResult('   exceptionDetails stack trace:');
      for (var i = 0; i < stack.length && i < 100; ++i) {
        TestRunner.addResult('       url: ' + stack[i].url);
        TestRunner.addResult('       function: ' + stack[i].functionName);
        TestRunner.addResult('       line: ' + stack[i].lineNumber);
      }
    }
  }

  var contextId = UIModule.Context.Context.instance().flavor(SDK.RuntimeModel.ExecutionContext).id;
  SourcesTestRunner.runDebuggerTestSuite([
    async function testSuccessfulCompileAndRun(next) {
      var expression = 'var a = 1; var b = 2; a + b; ';
      TestRunner.addResult('Compiling script');
      var response = await TestRunner.RuntimeAgent.invoke_compileScript(
          {expression, sourceURL: 'test.js', persistScript: true, executionContextId: contextId});

      TestRunner.assertTrue(!response.getError());
      TestRunner.assertTrue(!response.exceptionDetails);
      TestRunner.assertTrue(!!response.scriptId);

      TestRunner.addResult('Running script');
      response = await TestRunner.RuntimeAgent.invoke_runScript(
          {scriptId: response.scriptId, executionContextId: contextId, objectGroup: 'console', silent: false});
      TestRunner.assertTrue(!response.getError());
      TestRunner.assertTrue(!response.exceptionDetails);
      TestRunner.addResult('Script result: ' + response.result.value);
      next();
    },

    async function testRunError(next) {
      var expression = 'var a = 1; a + c; ';
      TestRunner.addResult('Compiling script');
      var response = await TestRunner.RuntimeAgent.invoke_compileScript(
          {expression, sourceURL: 'test.js', persistScript: true, executionContextId: contextId});
      TestRunner.assertTrue(!response.getError());
      TestRunner.assertTrue(!response.exceptionDetails);
      TestRunner.assertTrue(!!response.scriptId);

      TestRunner.addResult('Running script');
      response = await TestRunner.RuntimeAgent.invoke_runScript(
          {scriptId: response.scriptId, executionContextId: contextId, objectGroup: 'console', silent: false});
      TestRunner.assertTrue(!response.getError());
      TestRunner.assertTrue(!!response.exceptionDetails);
      printExceptionDetails(response.exceptionDetails);
      next();
    },

    async function testCompileError(next) {
      var expression = '}';
      TestRunner.addResult('Compiling script');
      var response = await TestRunner.RuntimeAgent.invoke_compileScript(
          {expression, sourceURL: 'test.js', persistScript: true, executionContextId: contextId});
      TestRunner.assertTrue(!response.getError());
      TestRunner.assertTrue(!!response.exceptionDetails);
      TestRunner.assertTrue(!response.scriptId);
      printExceptionDetails(response.exceptionDetails);
      next();
    }
  ]);
})();
