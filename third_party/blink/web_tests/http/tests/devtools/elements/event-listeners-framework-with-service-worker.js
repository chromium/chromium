// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
import * as SDK from 'devtools/core/sdk/sdk.js';
import * as BrowserDebugger from 'devtools/panels/browser_debugger/browser_debugger.js';

(async function() {
  TestRunner.addResult(`Tests framework event listeners output in Sources panel when service worker is present.\n`);
  await TestRunner.showPanel('elements');

  await TestRunner.evaluateInPage(`
    function testFunction() {}
  `);

  await TestRunner.loadHTML(`
      <body onload="testFunction()">
        <button id="inspectedNode">Inspect Me</button>
      </body>
    `);

  Common.Settings.settingForTest('show-event-listeners-for-ancestors').set(false);
  var scriptURL = 'http://127.0.0.1:8000/devtools/service-workers/resources/service-worker-empty.js';
  var scope = 'http://127.0.0.1:8000/devtools/service-workers/resources/scope1/';

  ApplicationTestRunner.waitForServiceWorker(step1);
  ApplicationTestRunner.registerServiceWorker(scriptURL, scope);

  var objectEventListenersPane;

  function isServiceWorker() {
    var target = UI.Context.Context.instance().flavor(SDK.RuntimeModel.ExecutionContext).target();
    return target.type() === SDK.Target.Type.ServiceWorker;
  }

  function step1(target) {
    SourcesTestRunner.waitForExecutionContextInTarget(target, step2);
  }

  async function step2(executionContext) {
    TestRunner.addResult('Selecting service worker thread');
    SourcesTestRunner.selectThread(executionContext.target());
    TestRunner.addResult('Context is service worker: ' + isServiceWorker());
    TestRunner.addResult('Dumping listeners');
    await UI.ViewManager.ViewManager.instance().showView('sources.global-listeners').then(() => {
      objectEventListenersPane = UI.Context.Context.instance().flavor(BrowserDebugger.ObjectEventListenersSidebarPane.ObjectEventListenersSidebarPane);
      objectEventListenersPane.update();
      ElementsTestRunner.expandAndDumpEventListeners(objectEventListenersPane.eventListenersView, step3);
    });
  }

  function step3() {
    TestRunner.addResult('Selecting main thread');
    SourcesTestRunner.selectThread(SDK.TargetManager.TargetManager.instance().primaryPageTarget());
    TestRunner.addResult('Context is service worker: ' + isServiceWorker());
    TestRunner.addResult('Dumping listeners');
    ElementsTestRunner.expandAndDumpEventListeners(objectEventListenersPane.eventListenersView, step4);
  }

  async function step4() {
    await ConsoleTestRunner.dumpConsoleMessages(false, false, TestRunner.textContentWithLineBreaks);
    TestRunner.completeTest();
  }
})();
