// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ExtensionsTestRunner} from 'extensions_test_runner';

(async function() {
  TestRunner.addResult(`Tests WebInspector extension API\n`);
  await TestRunner.loadHTML(`<iframe src="${TestRunner.url('../resources/extensions-frame-eval.html')}"></iframe>`);
  await ExtensionsTestRunner.runExtensionTests([
    function extension_testEvalInIFrame(nextTest) {
      var url = 'http://127.0.0.1:8000/devtools/resources/extensions-frame-eval.html';
      var origin = "http://127.0.0.1:8000"
      var options = {
        frameURL: url
      };
      var loc = "window.location.pathname";
      webInspector.inspectedWindow.eval(loc, options, callbackAndNextTest(extension_onEval, nextTest));
    },

    function extension_testEvalInIFrameBadOption(nextTest) {
      var url = 'http://127.0.0.1:8000/devtools/resources/extensions-frame-eval.html';
      var origin = "http://127.0.0.1:8000"
      var options = {
        frameURL: url,
        scriptExecutionContext: "bogus"
      };
      var loc = "window.location.pathname";
      webInspector.inspectedWindow.eval(loc, options, callbackAndNextTest(extension_onEval, nextTest));
    },

    function extension_onEval(value, isException) {
      output("Evaluate: " + JSON.stringify(value) + " (exception: " + isException + ")");
    }
  ]);
})();
