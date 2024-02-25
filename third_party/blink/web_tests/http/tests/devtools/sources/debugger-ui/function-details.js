// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests that Debugger.getFunctionDetails command returns correct location. Bug 71808\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
        function firstLineFunction()

        {
        }

        function notFirstLineFunction()

        {
        }

        var obj = {
            m: function() {}
        }

        function functionWithDisplayName() {}
        functionWithDisplayName.displayName = "user-friendly name";

        function functionWithDisplayNameGetter() {}
        functionWithDisplayNameGetter.__defineGetter__("displayName", function() { return "FAIL_should_not_execute"; });

        var smallClosure = (function(p) { return function() { return p; }; })("Capybara");

        var bigClosure = (function(p) {
            var o = {
               e: 7,
               f: 5,
               get u() { return 3; },
               set v(value) { }
            };
            with (o) {
                try {
                    throw Error("Test");
                } catch (ex) {
                    return function() {
                        return String(p) + String(ex) + u + e;
                    };
                }
            }
        })({});

        function* gen() { yield [1,2,3] }
    `);

  function dumpFunctionDetails(properties) {
    var location = properties.get('[[FunctionLocation]]').value.value;
    TestRunner.addResult('Function details: ');
    TestRunner.addResult('lineNumber: ' + location.lineNumber);
    TestRunner.addResult('columnNumber: ' + location.columnNumber);
    TestRunner.addResult('scriptId is valid: ' + !!location.scriptId);
    TestRunner.addResult('functionName: ' + properties.get('name').value.value);
    if (properties.has('[[IsGenerator]]'))
      TestRunner.addResult('isGenerator: ' + properties.get('[[IsGenerator]]').value.value);
  }

  async function performStandardTestCase(pageExpression, next) {
    var remote = await TestRunner.evaluateInPageRemoteObject(pageExpression);

    TestRunner.addResult(pageExpression + ' type = ' + remote.type);
    var response =
        await TestRunner.RuntimeAgent.invoke_getProperties({objectId: remote.objectId, isOwnProperty: false});

    var propertiesMap = new Map();
    for (var prop of response.internalProperties)
      propertiesMap.set(prop.name, prop);
    for (var prop of response.result) {
      if (prop.name === 'name' && prop.value && prop.value.type === 'string')
        propertiesMap.set('name', prop);
      if (prop.name === 'displayName' && prop.value && prop.value.type === 'string') {
        propertiesMap.set('name', prop);
        break;
      }
    }
    dumpFunctionDetails(propertiesMap);
    next();
  }

  SourcesTestRunner.runDebuggerTestSuite([
    function testGetFirstLineFunctionDetails(next) {
      performStandardTestCase('firstLineFunction', next);
    },
    function testGetNonFirstLineFunctionDetails(next) {
      performStandardTestCase('notFirstLineFunction', next);
    },
    function testGetDetailsOfFunctionWithInferredName(next) {
      performStandardTestCase('obj.m', next);
    },
    function testGetDetailsOfFunctionWithDisplayName(next) {
      performStandardTestCase('functionWithDisplayName', next);
    },
    function testGetDetailsOfFunctionWithDisplayNameGetter(next) {
      performStandardTestCase('functionWithDisplayNameGetter', next);
    },
    function testSmallClosure(next) {
      performStandardTestCase('smallClosure', next);
    },
    function testBigClosure(next) {
      performStandardTestCase('bigClosure', next);
    },
    function testGenFunction(next) {
      performStandardTestCase('gen', next);
    }
  ]);
})();
