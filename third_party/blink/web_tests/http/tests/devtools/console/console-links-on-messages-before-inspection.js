// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as UI from 'devtools/ui/legacy/legacy.js';
import * as Console from 'devtools/panels/console/console.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests a handling of a click on the link in a message, which had been shown before its originating script was added.\n`);

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    function loadScript()
    {
        var script = document.createElement('script');
        script.type = "text/javascript";
        script.src = "../resources/source2.js";
        document.body.appendChild(script);
    }
  `);


  var message = new SDK.ConsoleModel.ConsoleMessage(
      TestRunner.runtimeModel, Protocol.Log.LogEntrySource.JS,
      Protocol.Log.LogEntryLevel.Info, 'hello?',
      {url: 'http://127.0.0.1:8000/devtools/resources/source2.js'});

  const consoleModel = SDK.TargetManager.TargetManager.instance().primaryPageTarget().model(SDK.ConsoleModel.ConsoleModel);
  consoleModel.addMessage(message);
  TestRunner.debuggerModel.addEventListener(SDK.DebuggerModel.Events.ParsedScriptSource, onScriptAdded);
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.evaluateInPage('loadScript()');

  function onScriptAdded(event) {
    if (!event.data.contentURL().endsWith('source2.js'))
      return;

    TestRunner.addResult('script was added');
    var message = Console.ConsoleView.ConsoleView.instance().visibleViewMessages[0];
    var anchorElement = message.element().querySelector('.devtools-link');
    anchorElement.click();
  }

  InspectorFrontendHost.openInNewTab = function() {
    TestRunner.addResult('Failure: Open link in new tab!!');
    TestRunner.completeTest();
  };

  UI.InspectorView.InspectorView.instance().tabbedPane.addEventListener(UI.TabbedPane.Events.TabSelected, panelChanged);

  function panelChanged() {
    TestRunner.addResult('Panel ' + UI.InspectorView.InspectorView.instance().tabbedPane.currentTab.id + ' was opened');
    TestRunner.completeTest();
  }
})();
