// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that "Show Generator Location" jumps to the correct location.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function forward(iter, step)
      {
          while (step-- > 0)
              iter.next();
          return iter;
      }

      function* gen()
      {
          yield 1;
          yield 2;
          yield 3;
      }

      var iterNotStarted = gen();
      var iterSuspended1 = forward(gen(), 1);
      var iterSuspended2 = forward(gen(), 2);
      var iterSuspended3 = forward(gen(), 3);
      var iterClosed = forward(gen(), 4);
  `);

  var panel = UI.panels.sources;

  async function performStandardTestCase(pageExpression, next) {
    TestRunner.addSniffer(panel, 'showUISourceCode', showUISourceCodeHook);
    var remote = await TestRunner.evaluateInPageRemoteObject(pageExpression);

    remote.getOwnProperties().then(revealLocation.bind(null, remote));

    function revealLocation(remote, properties) {
      var loc;
      for (var prop of properties.internalProperties) {
        if (prop.name === '[[GeneratorLocation]]') {
          loc = prop.value.value;
          break;
        }
      }
      Common.Revealer.reveal(
          remote.debuggerModel().createRawLocationByScriptId(loc.scriptId, loc.lineNumber, loc.columnNumber));
    }

    function showUISourceCodeHook(uiSourceCode, lineNumber, columnNumber, forceShowInPanel) {
      // lineNumber and columnNumber are 0-based
      ++lineNumber;
      ++columnNumber;
      TestRunner.addResult('Generator location revealed: [' + lineNumber + ':' + columnNumber + ']');
      next();
    }
  }

  var expressions = [
    'iterNotStarted',
    'iterSuspended1',
    'iterSuspended2',
    'iterSuspended3',
    'iterClosed',
  ];

  function createTestSuiteFunction(expression) {
    var functionName = 'test' + expression.toTitleCase();
    return eval(
        'function ' + functionName + '(next)\n' +
        '{\n' +
        '    performStandardTestCase(\'' + expression + '\', next);\n' +
        '}; ' + functionName);
  }

  TestRunner.runTestSuite(expressions.map(createTestSuiteFunction));
})();
