// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ExtensionsTestRunner} from 'extensions_test_runner';

(async function() {
  TestRunner.addResult(`Tests WebInspector extension API\n`);
  await TestRunner.evaluateInPagePromise(`
    window.whereAmI = "main world";

    testRunner.setIsolatedWorldInfo(632, "http://devtools-extensions.oopif.test:8000", null);
    testRunner.evaluateScriptInIsolatedWorld(632, "window.whereAmI = 'brave new world'");
  `);
  await ExtensionsTestRunner.runExtensionTests([
    function extension_testEvalInMainWorldImplicit(nextTest) {
      webInspector.inspectedWindow.eval("whereAmI", callbackAndNextTest(extension_onEval, nextTest));
    },

    function extension_testEvalInMainWorldExplicit(nextTest) {
      webInspector.inspectedWindow.eval("whereAmI", { useContentScriptContext: false }, callbackAndNextTest(extension_onEval, nextTest));
    },

    function extension_testEvalInContentScriptContext(nextTest) {
      webInspector.inspectedWindow.eval("whereAmI", { useContentScriptContext: true }, callbackAndNextTest(extension_onEval, nextTest));
    },

    function extension_onEval(value, isException) {
      output("Evaluate: " + JSON.stringify(value) + " (exception: " + isException + ")");
    },
  ]);
})();
