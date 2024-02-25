// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as UIModule from 'devtools/ui/legacy/legacy.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests saving objects to temporary variables.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      for (var i = 3; i < 8; ++i)
          window["temp" + i] = "Reserved";
  `);

  var expressions = [
    '42', '\'foo string\'', 'NaN', 'Infinity', '-Infinity', '-0', '[1, 2, NaN, -0, null, undefined]',
    '({ foo: \'bar\' })', '(function(){ return arguments; })(1,2,3,4)', '(function func() {})', 'new Error(\'errr\')'
  ];

  TestRunner.addResult('Number of expressions: ' + expressions.length);
  TestRunner.addResult('Names [temp3..temp7] are reserved\n');

  function evaluateNext() {
    var expression = expressions.shift();
    if (!expression) {
      ConsoleTestRunner.waitForRemoteObjectsConsoleMessages(dumpConsoleMessages);
      return;
    }

    function didEvaluate(result) {
      TestRunner.assertTrue(!result.exceptionDetails, 'FAIL: was thrown. Expression: ' + expression);
      const consoleModel = SDK.TargetManager.TargetManager.instance().primaryPageTarget().model(SDK.ConsoleModel.ConsoleModel);
      consoleModel.saveToTempVariable(UIModule.Context.Context.instance().flavor(SDK.RuntimeModel.ExecutionContext), result.object);
      ConsoleTestRunner.waitUntilNthMessageReceived(2, evaluateNext);
    }

    UIModule.Context.Context.instance().flavor(SDK.RuntimeModel.ExecutionContext)
        .evaluate({expression: expression, objectGroup: 'console'})
        .then(didEvaluate);
  }

  async function dumpConsoleMessages() {
    await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
    TestRunner.completeTest();
  }

  evaluateNext();
})();
