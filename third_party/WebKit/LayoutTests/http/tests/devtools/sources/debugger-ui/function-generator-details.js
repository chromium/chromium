// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that Debugger.getGeneratorObjectDetails command returns correct result.\n`);
  await TestRunner.loadModule('sources_test_runner');
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

      var obj = {
          generator: function*()
          {
              yield 11;
              yield 12;
              yield 13;
          }
      };

      var iterNotStarted = gen();
      var iterSuspended = forward(gen(), 1);
      var iterClosed = forward(gen(), 5);

      var iterObjGenerator = forward(obj.generator(), 2);

      var anonymousGenIter = (function*() {
          yield 21;
          yield 22;
          yield 23;
      })();
      forward(anonymousGenIter, 3);
  `);

  function performStandardTestCase(pageExpression, next) {
    UI.context.flavor(SDK.ExecutionContext)
        .evaluate({expression: pageExpression})
        .then(didEvaluate);

    function didEvaluate({object}) {
      TestRunner.addResult(
          pageExpression + ': type = ' + object.type +
          ', subtype = ' + object.subtype);
      object.getOwnProperties().then(dumpInternalProperties);
    }

    function dumpInternalProperties(properties) {
      TestRunner.assertTrue(
          properties && properties.internalProperties, 'FAIL: no properties');
      for (var prop of properties.internalProperties) {
        if (prop.name !== '[[GeneratorLocation]]')
          TestRunner.addResult(prop.name + ' = ' + prop.value.description);
        else
          dumpLocation(prop.value.value);
      }
      next();
    }

    function dumpLocation(location) {
      TestRunner.addResult('lineNumber = ' + location.lineNumber);
      TestRunner.addResult('columnNumber = ' + location.columnNumber);
      TestRunner.addResult(
          'script is valid: ' + (location.scriptId ? 'yes' : 'no'));
    }
  }

  var expressions = [
    'iterNotStarted',
    'iterSuspended',
    'iterClosed',
    'iterObjGenerator',
    'anonymousGenIter',
  ];

  function createTestSuiteFunction(expression) {
    var functionName = 'test' + expression.toTitleCase();
    return eval(
        'function ' + functionName + '(next)\n' +
        '{\n' +
        '    performStandardTestCase(\'' + expression + '\', next);\n' +
        '}; ' + functionName);
  }

  SourcesTestRunner.runDebuggerTestSuite(
      expressions.map(createTestSuiteFunction));
})();
