// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests how execution context and target are selected.\n`);
  await TestRunner.loadTestModule('sdk_test_runner');
  await TestRunner.loadLegacyModule('main');
  await TestRunner.showPanel('sources');

  var context = new UI.Context();
  context.addFlavorChangeListener(SDK.ExecutionContext, executionContextChanged, this);
  context.addFlavorChangeListener(SDK.Target, targetChanged, this);
  new Main.ExecutionContextSelector(SDK.targetManager, context);

  function executionContextChanged(event) {
    var executionContext = event.data;
    TestRunner.addResult(
        'Execution context selected: ' +
        (executionContext.isDefault ? executionContext.target().name() :
                                      executionContext.name));
  }

  function targetChanged(event) {
    TestRunner.addResult('Target selected: ' + event.data.name());
  }

  TestRunner.addResult('');
  TestRunner.addResult('Adding page target');
  var pageMock = new SDKTestRunner.PageMock('mock-url.com/page.html');
  var pageTarget = pageMock.connectAsMainTarget('page-target');
  await pageTarget.model(SDK.RuntimeModel).once(SDK.RuntimeModel.Events.ExecutionContextCreated);
  pageMock.evalScript('contentScript1.js', 'var script', true /* isContentScript */);
  await pageTarget.model(SDK.RuntimeModel).once(SDK.RuntimeModel.Events.ExecutionContextCreated);

  TestRunner.addResult('');
  TestRunner.addResult('Adding frame target');
  var frameMock = new SDKTestRunner.PageMock('mock-url.com/iframe.html');
  var frameTarget = frameMock.connectAsChildTarget('frame-target', pageMock);
  await frameTarget.model(SDK.RuntimeModel).once(SDK.RuntimeModel.Events.ExecutionContextCreated);

  TestRunner.addResult('');
  TestRunner.addResult('Adding worker target');
  var workerMock = new SDKTestRunner.PageMock('mock-url.com/worker.js');
  workerMock.turnIntoWorker();
  var workerTarget = workerMock.connectAsChildTarget('worker-target', pageMock);
  await workerTarget.model(SDK.RuntimeModel).once(SDK.RuntimeModel.Events.ExecutionContextCreated);

  TestRunner.addResult('');
  TestRunner.addResult('User selected content script');
  context.setFlavor(SDK.ExecutionContext, pageTarget.model(SDK.RuntimeModel).executionContexts().find(context => !context.isDefault));

  TestRunner.addResult('');
  TestRunner.addResult('Removing content script');
  pageMock.removeContentScripts();
  await pageTarget.model(SDK.RuntimeModel).once(SDK.RuntimeModel.Events.ExecutionContextDestroyed);

  TestRunner.addResult('');
  TestRunner.addResult('Readding content script');
  pageMock.evalScript('contentScript2.js', 'var script', true /* isContentScript */);
  await pageTarget.model(SDK.RuntimeModel).once(SDK.RuntimeModel.Events.ExecutionContextCreated);

  TestRunner.addResult('');
  TestRunner.addResult('Switching to worker target');
  context.setFlavor(SDK.Target, workerTarget);

  TestRunner.addResult('');
  TestRunner.addResult('Switching to page target');
  context.setFlavor(SDK.Target, pageTarget);

  TestRunner.addResult('');
  TestRunner.addResult('User selected content script');
  context.setFlavor(SDK.ExecutionContext, pageTarget.model(SDK.RuntimeModel).executionContexts().find(context => !context.isDefault));

  TestRunner.addResult('');
  TestRunner.addResult('Switching to worker target');
  context.setFlavor(SDK.Target, workerTarget);

  TestRunner.addResult('');
  TestRunner.addResult('Switching to page target');
  context.setFlavor(SDK.Target, pageTarget);

  TestRunner.addResult('');
  TestRunner.addResult('User selected iframe1');
  context.setFlavor(SDK.ExecutionContext, frameTarget.model(SDK.RuntimeModel).executionContexts()[0]);

  TestRunner.addResult('');
  TestRunner.addResult('Switching to worker target');
  context.setFlavor(SDK.Target, workerTarget);

  TestRunner.addResult('');
  TestRunner.addResult('Removing worker');
  workerMock.disconnect();

  TestRunner.addResult('');
  TestRunner.addResult('User selected content script');
  context.setFlavor(SDK.ExecutionContext, pageTarget.model(SDK.RuntimeModel).executionContexts().find(context => !context.isDefault));

  TestRunner.addResult('');
  TestRunner.addResult('Switching to iframe target');
  context.setFlavor(SDK.Target, frameTarget);

  TestRunner.addResult('');
  TestRunner.addResult('Removing content script');
  pageMock.removeContentScripts();
  await pageTarget.model(SDK.RuntimeModel).once(SDK.RuntimeModel.Events.ExecutionContextDestroyed);

  TestRunner.addResult('');
  TestRunner.addResult('Switching to page target');
  context.setFlavor(SDK.Target, pageTarget);

  TestRunner.completeTest();
})();
