// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  'use strict';
  TestRunner.addResult(
    `Tests that evaluating 'console.log()' in the console will have access to its outer scope variables. Bug 60547.\n`
  );

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    // Used to interfere into InjectedScript._propertyDescriptors()
    Object.prototype.get = function() { return "FAIL"; };
    Object.prototype.set = function() { return "FAIL"; };
    Object.prototype.value = "FAIL";
    Object.prototype.getter = "FAIL";
    Object.prototype.setter = "FAIL";
    Object.prototype.isOwn = true;
    // Used to interfere into InjectedScript.primitiveTypes
    Object.prototype.object = true;
    // Used to interfere into InjectedScript.getEventListeners()
    Object.prototype.nullValue = null;
    Object.prototype.undefValue = undefined;

    var foo = "bar";
    var testObj = {
      get getter() { },
      set setter(_) { },
      baz: "baz"
    };
  `);

  // Use `new Function` as with-statements are not allowed in strict-mode
  const snippet1 = new Function(`
(function(obj) {
  with (obj) {
    console.log('with: ' + a);
    eval("console.log('eval in with: ' + a)");
  }
})({ a: 'Object property value' });`);

  function snippet2() {
    (function(a) {
      eval("console.log('eval in function: ' + a)");
    })('Function parameter');
  }

  function bodyText(f) {
    var text = f.toString();
    var begin = text.indexOf('{');
    return text.substring(begin);
  }

  function dumpAndClearConsoleMessages(next) {
    TestRunner.deprecatedRunAfterPendingDispatches(async function() {
      await ConsoleTestRunner.dumpConsoleMessages();
      Console.ConsoleView.ConsoleView.clearConsole();
      TestRunner.deprecatedRunAfterPendingDispatches(next);
    });
  }

  TestRunner.runTestSuite([
    function testSnippet1(next) {
      TestRunner.evaluateInPage(bodyText(snippet1), dumpAndClearConsoleMessages.bind(null, next));
    },

    function testSnippet2(next) {
      TestRunner.evaluateInPage(bodyText(snippet2), dumpAndClearConsoleMessages.bind(null, next));
    },

    function testConsoleEvalPrimitiveValue(next) {
      ConsoleTestRunner.evaluateInConsole('foo', dumpAndClearConsoleMessages.bind(null, next));
    },

    async function testConsoleEvalObject(next) {
      var result = await TestRunner.RuntimeAgent.evaluate('testObj');
      var properties = await TestRunner.RuntimeAgent.getProperties(result.objectId, /* isOwnProperty */ true);
      for (var p of properties)
        TestRunner.dump(p, { objectId: 'formatAsTypeName', description: 'formatAsDescription' });
      next();
    },

    function testGetEventListenersDoesNotThrow(next) {
      ConsoleTestRunner.evaluateInConsole(
        'getEventListeners(document.body.firstChild)',
        dumpAndClearConsoleMessages.bind(null, next)
      );
    }
  ]);
})();
