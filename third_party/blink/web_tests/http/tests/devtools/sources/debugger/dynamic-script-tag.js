// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that inline scripts and document.write scripts get different uiSourceCodes with different URLs.`);
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise('resources/dynamic-script-tag.html');

  SourcesTestRunner.runDebuggerTestSuite([
    function testOpenDevToolsAfterLoad(next) {
      var consoleMessagesCount = 2;

      var messages = SDK.ConsoleModel.ConsoleModel.allMessagesUnordered();
      for (var i = 0; i < messages.length; ++i)
        processMessage(messages[i]);
      checkWhenScriptsLoaded();

      function processMessage(message) {
        if (message.messageText.indexOf('DYNAMIC') !== -1)
          --consoleMessagesCount;
      }

      function consoleMessageAppended(message) {
        processMessage(message);
        checkWhenScriptsLoaded();
      }

      function checkWhenScriptsLoaded() {
        if (consoleMessagesCount) {
          ConsoleTestRunner.addConsoleSniffer(consoleMessageAppended);
          return;
        }

        TestRunner.addResult('Both dynamic scripts were loaded.');
        var scripts = SourcesTestRunner.queryScripts();

        function filterOutInlineAndStandaloneScripts(script) {
          return !script.isInlineScript() &&
              script.sourceURL.indexOf('dynamic-script-tag.html') !== -1;
        }
        scripts = scripts.filter(filterOutInlineAndStandaloneScripts);
        TestRunner.addResult(
            'Number of non-inline scripts with document url as a sourceURL: ' +
            scripts.length + '.');
        next();
      }
    },

    function testOpenDevToolsThenReload(next) {
      TestRunner.debuggerModel.addEventListener(
          SDK.DebuggerModel.Events.ParsedScriptSource, scriptParsed);
      TestRunner.addResult('Reloading page.');
      TestRunner.reloadPage(onPageReloaded);

      var eventsCountBeforeNext = 2;
      function onPageReloaded() {
        if (!--eventsCountBeforeNext)
          next();
      }

      function scriptParsed(event) {
        var script = event.data;
        if (script.sourceURL.indexOf('dynamic-script-tag.html') !== -1) {
          // Both dynamic scripts will be loaded before inline script on reload,
          // so we are just checking that the first one that has document url as
          // a sourceURL is inline.
          TestRunner.addResult(
              'The first script with document url as a sourceURL to be seen is ' +
              (script.isInlineScript() ? '' : 'not ') + 'inline script.');
          TestRunner.assertTrue(
              script.isInlineScript(),
              'Only inline scripts should have document url as a sourceURL.');
          TestRunner.debuggerModel.removeEventListener(
              SDK.DebuggerModel.Events.ParsedScriptSource, scriptParsed);
          if (!--eventsCountBeforeNext)
            next();
        }
      }
    },
  ]);
})();
