// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ExtensionsTestRunner} from 'extensions_test_runner';

(async function() {
  TestRunner.addResult(`Tests WebInspector extension API\n`);
  await TestRunner.evaluateInPagePromise(`
    window.inspectedValue = { str: "foo", num: 42 };

    window.loop = { };
    window.loop.next = window.loop;
  `);

  await ExtensionsTestRunner.runExtensionTests([
    function extension_testEvalOk(nextTest) {
      webInspector.inspectedWindow.eval("inspectedValue", callbackAndNextTest(extension_onEval, nextTest));
    },

    function extension_testEvalStringifyingLoopFailed(nextTest) {
      webInspector.inspectedWindow.eval("window.loop", callbackAndNextTest(extension_onEval, nextTest));
    },

    function extension_testEvalDefinesGlobalSymbols(nextTest) {
      webInspector.inspectedWindow.eval("function extensionFunc() {}");
      webInspector.inspectedWindow.eval("extensionVar = 42;");
      webInspector.inspectedWindow.eval("({ func: typeof window.extensionFunc, variable: window.extensionVar })", callbackAndNextTest(extension_onEval, nextTest));
    },

    function extension_testEvalStatement(nextTest) {
      webInspector.inspectedWindow.eval("var x = 3; while (--x); x", callbackAndNextTest(extension_onEval, nextTest));
    },

    function extension_testEvalUndefined(nextTest) {
      webInspector.inspectedWindow.eval("undefined", callbackAndNextTest(extension_onEval, nextTest));
    },

    function extension_testEvalConsoleAPI(nextTest) {
      webInspector.inspectedWindow.eval("typeof inspect", callbackAndNextTest(extension_onEval, nextTest));
    },

    function extension_testWithStringifyOverridden(nextTest) {
      webInspector.inspectedWindow.eval("(JSON.stringify = function() { throw 'oops! you can not use JSON.stringify'; }), 'OK'", callbackAndNextTest(extension_onEval, nextTest));
    },

    function extension_testEvalThrows(nextTest) {
      webInspector.inspectedWindow.eval("throw('testExceptionString')", callbackAndNextTest(extension_onEval, nextTest));
    },

    function extension_onEval(value, status) {
      var exceptionString = typeof status === "undefined" ? "undefined" : JSON.stringify(status);
      output("Evaluate: " + JSON.stringify(value) + " (exception: " + exceptionString + ")");
    },
  ]);
})();
