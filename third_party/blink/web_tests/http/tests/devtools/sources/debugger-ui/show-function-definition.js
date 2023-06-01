// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that "Show Function Definition" jumps to the correct location.\n`);
  await TestRunner.loadLegacyModule('console');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function jumpToMe()
      {
          var result = 12345;
          return window.foo || result;
      }
  `);

  var panel = UI.panels.sources;

  TestRunner.runTestSuite([
    function testRevealFunctionDefinition(next) {
      TestRunner.addSniffer(panel, 'showUISourceCode', showUISourceCodeHook);
      UI.context.flavor(SDK.ExecutionContext).evaluate({expression: 'jumpToMe', silent: true}).then(didGetFunction);

      function didGetFunction(result) {
        var error = !result.object || !!result.exceptionDetails;
        TestRunner.assertTrue(!error);
        panel.showFunctionDefinition(result.object);
      }

      function showUISourceCodeHook(uiSourceCode, lineNumber, columnNumber, forceShowInPanel) {
        // lineNumber and columnNumber are 0-based
        ++lineNumber;
        ++columnNumber;
        TestRunner.addResult('Function location revealed: [' + lineNumber + ':' + columnNumber + ']');
        next();
      }
    },

    function testDumpFunctionDefinition(next) {
      TestRunner.addSniffer(ObjectUI.ObjectPropertiesSection, 'formatObjectAsFunction', onConsoleMessagesReceived);
      var consoleView = Console.ConsoleView.instance();
      consoleView.prompt.appendCommand('jumpToMe', true);

      function onConsoleMessagesReceived() {
        TestRunner.deprecatedRunAfterPendingDispatches(function() {
          var messages = [];
          ConsoleTestRunner.disableConsoleViewport();
          var viewMessages = Console.ConsoleView.instance().visibleViewMessages;
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
