// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests saving objects to temporary variables.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
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
      const consoleModel = SDK.targetManager.primaryPageTarget().model(SDK.ConsoleModel);
      consoleModel.saveToTempVariable(UI.context.flavor(SDK.ExecutionContext), result.object);
      ConsoleTestRunner.waitUntilNthMessageReceived(2, evaluateNext);
    }

    UI.context.flavor(SDK.ExecutionContext)
        .evaluate({expression: expression, objectGroup: 'console'})
        .then(didEvaluate);
  }

  async function dumpConsoleMessages() {
    await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
    TestRunner.completeTest();
  }

  evaluateNext();
})();
