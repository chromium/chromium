// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ExtensionsTestRunner} from 'extensions_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';

(async function() {
  TestRunner.addResult(
      `Tests that webInspector.inspectedWindow.reload() successfully injects and preprocesses user's code upon reload\n`);
  await TestRunner.navigatePromise(TestRunner.url('resources/reload.html'));

  TestRunner.lastMessageScriptId = function(callback) {
    var consoleView = Console.ConsoleView.ConsoleView.instance();
    if (consoleView.needsFullUpdate)
      consoleView.updateMessageList();
    var viewMessages = consoleView.visibleViewMessages;
    if (viewMessages.length !== 1)
      callback(null);
    var uiMessage = viewMessages[viewMessages.length - 1];
    var message = uiMessage.consoleMessage();
    if (!message.stackTrace)
      callback(null);
    callback(message.stackTrace.callFrames[0].scriptId);
  }
  TestRunner.getScriptSource = async function(scriptId, callback) {
    var source = await TestRunner.DebuggerAgent.getScriptSource(scriptId);
    callback(source);
  }

  await ExtensionsTestRunner.runExtensionTests([
    function extension_testReloadInjectsCode(nextTest) {
      var injectedValue;

      function onPageWithInjectedCodeLoaded() {
        webInspector.inspectedWindow.eval("window.bar", function(value) {
          injectedValue = value;
          evaluateOnFrontend("TestRunner.reloadPage(reply)", onPageWithoutInjectedCodeLoaded);
        });
      }
      function onPageWithoutInjectedCodeLoaded() {
        webInspector.inspectedWindow.eval("window.bar", function(value) {
          output("With injected code: " + injectedValue);
          output("Without injected code: " + value);
          nextTest();
        });
      }
      var injectedScript = "window.foo = 42;"
      evaluateOnFrontend(`TestRunner.reloadPageWithInjectedScript("${injectedScript}", reply)`, onPageWithInjectedCodeLoaded);
    },

    function extension_testReloadInjectsCodeWithMessage(nextTest) {
      function onPageWithInjectedCodeLoaded() {
        evaluateOnFrontend("TestRunner.lastMessageScriptId(reply);", onScriptIdReceived);
      }

      function onScriptIdReceived(scriptId) {
        if (!scriptId) {
          output("Script ID unavailable");
          nextTest();
        } else {
          evaluateOnFrontend("TestRunner.getScriptSource(\"" + scriptId + "\", reply);", function(source) {
            output("Source received:");
            output(source);
            nextTest();
          });
        }
      }

      var injectedScript = "console.log(42)";
      evaluateOnFrontend(`TestRunner.reloadPageWithInjectedScript("${injectedScript}", reply)`, onPageWithInjectedCodeLoaded);
    }
  ]);
})();
