// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as UIModule from 'devtools/ui/legacy/legacy.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests saving objects to temporary variables while paused.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      var thisObj;

      function testFunction()
      {
          function inner()
          {
              debugger;
          }

          thisObj = { foo: 42, __proto__: null };
          for (var i = 1; i < 6; ++i)
              thisObj["temp" + i] = "FAIL";

          inner.call(thisObj);
      }

      function checkThisObject()
      {
          for (var key in thisObj) {
              if (key === "foo")
                  console.assert(thisObj[key] === 42);
              else if (key.substr(0, 4) === "temp")
                  console.assert(thisObj[key] === "FAIL");
              else
                  console.error("FAIL: Unexpected property " + key);
          }
      }

      for (var i = 3; i < 8; ++i)
          window["temp" + i] = "Reserved";
  `);

  var expressions = [
    '42', '\'foo string\'', 'NaN', 'Infinity', '-Infinity', '-0', '[1, 2, NaN, -0, null, undefined]',
    '({ foo: \'bar\' })', '(function(){ return arguments; })(1,2,3,4)', '(function func() {})', 'new Error(\'errr\')'
  ];

  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.startDebuggerTest(step1);

  TestRunner.addResult('Number of expressions: ' + expressions.length);
  TestRunner.addResult('Names [temp3..temp7] are reserved\n');

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(didPause);
  }

  function didPause() {
    evaluateNext();
  }

  function evaluateNext() {
    var expression = expressions.shift();
    if (!expression) {
      ConsoleTestRunner.waitForRemoteObjectsConsoleMessages(tearDown);
      return;
    }

    function didEvaluate(result) {
      TestRunner.assertTrue(!result.exceptionDetails, 'FAIL: was thrown. Expression: ' + expression);
      SDK.consoleModel.saveToTempVariable(UIModule.Context.Context.instance().flavor(SDK.RuntimeModel.RuntimeModel.ExecutionContext), result.object);
      ConsoleTestRunner.waitUntilNthMessageReceived(2, evaluateNext);
    }

    UIModule.Context.Context.instance().flavor(SDK.RuntimeModel.RuntimeModel.ExecutionContext)
        .evaluate({expression: expression, objectGroup: 'console'})
        .then(didEvaluate);
  }

  function tearDown() {
    TestRunner.evaluateInPage('checkThisObject()', dumpConsoleMessages);
  }

  async function dumpConsoleMessages() {
    await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
    SourcesTestRunner.completeDebuggerTest();
  }
})();
