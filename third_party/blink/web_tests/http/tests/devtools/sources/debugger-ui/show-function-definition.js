// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Console from 'devtools/panels/console/console.js';
import * as ObjectUI from 'devtools/ui/legacy/components/object_ui/object_ui.js';
import * as Sources from 'devtools/panels/sources/sources.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that "Show Function Definition" jumps to the correct location.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function jumpToMe()
      {
          var result = 12345;
          return window.foo || result;
      }
  `);

  var panel = Sources.SourcesPanel.SourcesPanel.instance();

  TestRunner.runTestSuite([
    function testRevealFunctionDefinition(next) {
      TestRunner.addSniffer(panel, 'showUISourceCode', showUISourceCodeHook);
      UI.Context.Context.instance().flavor(SDK.RuntimeModel.ExecutionContext).evaluate({expression: 'jumpToMe', silent: true}).then(didGetFunction);

      function didGetFunction(result) {
        var error = !result.object || !!result.exceptionDetails;
        TestRunner.assertTrue(!error);
        panel.showFunctionDefinition(result.object);
      }

      function showUISourceCodeHook(uiSourceCode, {lineNumber, columnNumber}, forceShowInPanel) {
        // lineNumber and columnNumber are 0-based
        ++lineNumber;
        ++columnNumber;
        TestRunner.addResult('Function location revealed: [' + lineNumber + ':' + columnNumber + ']');
        next();
      }
    },

    function testDumpFunctionDefinition(next) {
      TestRunner.addSniffer(ObjectUI.ObjectPropertiesSection.ObjectPropertiesSection, 'formatObjectAsFunction', onConsoleMessagesReceived);
      var consoleView = Console.ConsoleView.ConsoleView.instance();
      consoleView.prompt.appendCommand('jumpToMe', true);

      function onConsoleMessagesReceived() {
        TestRunner.deprecatedRunAfterPendingDispatches(function() {
          var messages = [];
          ConsoleTestRunner.disableConsoleViewport();
          var viewMessages = Console.ConsoleView.ConsoleView.instance().visibleViewMessages;
          for (var i = 0; i < viewMessages.length; ++i) {
            var uiMessage = viewMessages[i];
            var element = uiMessage.contentElement();
            messages.push(element.deepTextContent());
          }
          TestRunner.addResult(messages.join('\n'));
          next();
        });
      }
    }
  ]);
})();
