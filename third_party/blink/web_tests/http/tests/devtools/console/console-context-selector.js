// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests console execution context selector.\n`);

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    function setup() {
      window.worker = new Worker('resources/worker-pause.js');
      window.iframe = document.createElement('iframe');
      window.iframe.src = '../resources/inspected-page.html';
      window.iframe.name = 'myframe';
      document.body.appendChild(window.iframe);
      return new Promise(f => window.iframe.onload = f);
    }

    function pauseInWorker() {
      window.worker.postMessage('pause');
    }

    function pauseInIframe() {
      window.iframe.contentWindow.eval('debugger;');
    }

    function pauseInMain() {
      debugger;
    }

    function destroyIframe() {
      window.iframe.parentElement.removeChild(window.iframe);
      window.iframe = null;
    }
  `);

  await new Promise(f => SourcesTestRunner.startDebuggerTest(f, true));
  await TestRunner.evaluateInPageAsync('setup()');
  var workerTarget = await TestRunner.waitForTarget(target => target.parentTarget() === TestRunner.mainTarget);
  await TestRunner.waitForExecutionContext(workerTarget.model(SDK.RuntimeModel));
  dump();

  TestRunner.addResult('');
  TestRunner.addResult('Selected worker');
  UI.context.setFlavor(SDK.Target, workerTarget);
  dump();

  var childFrame =
      TestRunner.resourceTreeModel.frames().find(frame => frame !== TestRunner.resourceTreeModel.mainFrame);
  var childExecutionContext =
      TestRunner.runtimeModel.executionContexts().find(context => context.frameId === childFrame.id);
  TestRunner.addResult('');
  TestRunner.addResult('Selected iframe');
  UI.context.setFlavor(SDK.ExecutionContext, childExecutionContext);
  dump();

  var handleExecutionContextFlavorChanged;
  UI.context.addFlavorChangeListener(SDK.DebuggerModel.CallFrame, () => {
    if (handleExecutionContextFlavorChanged)
      handleExecutionContextFlavorChanged();
    handleExecutionContextFlavorChanged = undefined;
  });

  function waitForExecutionContextFlavorChanged() {
    return new Promise(fulfill => {
      handleExecutionContextFlavorChanged = fulfill;
    });
  }

  TestRunner.evaluateInPage('pauseInMain()');
  await SourcesTestRunner.waitUntilPausedPromise();
  await waitForExecutionContextFlavorChanged();
  TestRunner.addResult('');
  TestRunner.addResult('Paused in main');
  dump();

  await new Promise(f => SourcesTestRunner.resumeExecution(f));
  await waitForExecutionContextFlavorChanged();
  TestRunner.addResult('');
  TestRunner.addResult('Resumed');
  dump();

  TestRunner.evaluateInPage('pauseInWorker()');
  await SourcesTestRunner.waitUntilPausedPromise();
  await waitForExecutionContextFlavorChanged();
  TestRunner.addResult('');
  TestRunner.addResult('Paused in worker');
  dump();

  await new Promise(f => SourcesTestRunner.resumeExecution(f));
  await waitForExecutionContextFlavorChanged();
  TestRunner.addResult('');
  TestRunner.addResult('Resumed');
  dump();

  TestRunner.evaluateInPage('pauseInIframe()');
  await SourcesTestRunner.waitUntilPausedPromise();
  await waitForExecutionContextFlavorChanged();
  TestRunner.addResult('');
  TestRunner.addResult('Paused in iframe');
  dump();

  await new Promise(f => SourcesTestRunner.resumeExecution(f));
  await waitForExecutionContextFlavorChanged();
  TestRunner.addResult('');
  TestRunner.addResult('Resumed');
  dump();

  TestRunner.evaluateInPage('destroyIframe()');
  await TestRunner.waitForExecutionContextDestroyed(childExecutionContext);
  TestRunner.addResult('');
  TestRunner.addResult('Destroyed iframe');
  dump();

  SourcesTestRunner.completeDebuggerTest();

  function dump() {
    var consoleView = Console.ConsoleView.instance();
    var selector = consoleView.consoleContextSelector;
    TestRunner.addResult('Console context selector:');

    for (var executionContext of selector._items) {
      var selected = UI.context.flavor(SDK.ExecutionContext) === executionContext;
      var text = '____'.repeat(selector.depthFor(executionContext)) + selector.titleFor(executionContext);
      var disabled = !selector.isItemSelectable(executionContext);
      TestRunner.addResult(`${selected ? '*' : ' '} ${text} ${disabled ? '[disabled]' : ''}`);
    }
  }
})();
