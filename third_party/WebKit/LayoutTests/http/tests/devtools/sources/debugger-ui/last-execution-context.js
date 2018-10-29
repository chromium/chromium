// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests how execution context and target are selected.\n`);
  await TestRunner.showPanel('sources');

  var mockTargetId = 1;
  function createMockTarget(name, capabilities, dontAttachToMain) {
    return SDK.targetManager.createTarget(
        'mock-target-' + mockTargetId++, name, capabilities, params => new SDK.StubConnection(params),
        dontAttachToMain ? null : TestRunner.mainTarget);
  }

  var context = new UI.Context();
  context.addFlavorChangeListener(SDK.ExecutionContext, executionContextChanged, this);
  context.addFlavorChangeListener(SDK.Target, targetChanged, this);
  new Main.ExecutionContextSelector(SDK.targetManager, context);

  function executionContextChanged(event) {
    var executionContext = event.data;
    TestRunner.addResult(
        'Execution context selected: ' +
        (executionContext.isDefault ? executionContext.target().name() + ':' + executionContext.frameId :
                                      executionContext.name));
  }

  function targetChanged(event) {
    TestRunner.addResult('Target selected: ' + event.data.name());
  }

  TestRunner.runtimeModel._executionContextsCleared();


  TestRunner.addResult('');
  TestRunner.addResult('Adding page target');
  var pageTarget = createMockTarget('page-target', SDK.Target.Capability.AllForTests, true /* dontAttachToMain */);
  var pageRuntimeModel = pageTarget.model(SDK.RuntimeModel);
  pageTarget.model(SDK.ResourceTreeModel)._frameAttached('42', '');
  pageTarget.model(SDK.ResourceTreeModel)._frameNavigated({
    id: '42',
    parentId: '',
    loaderId: '',
    name: 'mock-frame',
    url: 'mock-url.com/frame.html',
    securityOrigin: 'mock-security-origin',
    mineType: 'mimeType'
  });
  pageRuntimeModel._executionContextCreated(
      {id: 'cs1', auxData: {isDefault: false, frameId: '42'}, origin: 'origin', name: 'contentScript1'});
  pageRuntimeModel._executionContextCreated(
      {id: 'if1', auxData: {isDefault: true, frameId: 'iframe1'}, origin: 'origin', name: 'iframeContext1'});
  pageRuntimeModel._executionContextCreated(
      {id: 'p1', auxData: {isDefault: true, frameId: '42'}, origin: 'origin', name: 'pageContext1Name'});

  TestRunner.addResult('');
  TestRunner.addResult('Adding sw target');
  var swTarget = createMockTarget(
      'sw-target', SDK.Target.Capability.Network | SDK.Target.Capability.Worker | SDK.Target.Capability.JS);
  swTarget.model(SDK.RuntimeModel)
      ._executionContextCreated(
          {id: 'sw1', auxData: {isDefault: true, frameId: ''}, origin: 'origin', name: 'swContext1Name'});

  TestRunner.addResult('');
  TestRunner.addResult('Removing page main frame');
  pageRuntimeModel._executionContextDestroyed('p1');

  TestRunner.addResult('');
  TestRunner.addResult('Readding page main frame');
  pageRuntimeModel._executionContextCreated(
      {id: 'p2', auxData: {isDefault: true, frameId: '42'}, origin: 'origin', name: 'pageContext1Name'});

  TestRunner.addResult('');
  TestRunner.addResult('Switching to sw target');
  context.setFlavor(SDK.Target, swTarget);

  TestRunner.addResult('');
  TestRunner.addResult('Switching to page target');
  context.setFlavor(SDK.Target, pageTarget);

  TestRunner.addResult('');
  TestRunner.addResult('User selected content script');
  context.setFlavor(SDK.ExecutionContext, pageRuntimeModel.executionContexts().find(context => context.id === 'cs1'));

  TestRunner.addResult('');
  TestRunner.addResult('Switching to sw target');
  context.setFlavor(SDK.Target, swTarget);

  TestRunner.addResult('');
  TestRunner.addResult('Switching to page target');
  context.setFlavor(SDK.Target, pageTarget);

  TestRunner.addResult('');
  TestRunner.addResult('User selected iframe1');
  context.setFlavor(SDK.ExecutionContext, pageRuntimeModel.executionContexts().find(context => context.id === 'if1'));

  TestRunner.addResult('');
  TestRunner.addResult('Switching to sw target');
  context.setFlavor(SDK.Target, swTarget);

  TestRunner.addResult('');
  TestRunner.addResult('Switching to page target');
  context.setFlavor(SDK.Target, pageTarget);

  TestRunner.addResult('');
  TestRunner.addResult('Switching to sw target');
  context.setFlavor(SDK.Target, swTarget);

  TestRunner.addResult('');
  TestRunner.addResult('Removing page main frame');
  pageRuntimeModel._executionContextDestroyed('p2');

  TestRunner.addResult('');
  TestRunner.addResult('Readding page main frame');
  pageRuntimeModel._executionContextCreated(
      {id: 'p3', auxData: {isDefault: true, frameId: '42'}, origin: 'origin', name: 'pageContext1Name'});

  TestRunner.completeTest();
})();
