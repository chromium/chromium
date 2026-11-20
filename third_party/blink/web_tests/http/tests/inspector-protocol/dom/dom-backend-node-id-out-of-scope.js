// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
    <input aria-label="inspected-field" value="inspected-value">
    <template><span>template-content</span></template>
  `,
      'Tests that backendNodeId cannot resolve nodes of another target sharing the renderer process.');

  const contextCreatedPromise = dp.Runtime.onceExecutionContextCreated(
      event => event.params.context.auxData.isDefault);
  await dp.Runtime.enable();
  const executionContextId = (await contextCreatedPromise).params.context.id;

  // Create a same-origin subframe with both a normal element and <template>
  // content, retaining JS references so Oilpan keeps both alive after removal.
  await session.evaluate(`
    const iframe = document.createElement('iframe');
    document.body.appendChild(iframe);
    iframe.contentDocument.body.innerHTML =
        '<input aria-label="iframe-field">' +
        '<template><span>iframe-template</span></template>';
    window.retainedIframeInput =
        iframe.contentDocument.querySelector('input');
    window.retainedIframeTemplateSpan =
        iframe.contentDocument.querySelector('template').content.firstChild;
    undefined;
  `);

  // Open a same-origin popup: it shares the renderer process (and thus the
  // process-global backend node ids) but is a separate target.
  const target = testRunner.browserP().Target;
  await target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  // Auto-attach also reports pre-existing targets; wait for the popup only.
  const attachedPromise =
      target.onceAttachedToTarget(event => !!event.params.targetInfo.openerId);
  // Don't await window.open before runIfWaitingForDebugger(): its response
  // only arrives once the paused popup target resumes.
  const openPromise = session.evaluate(`
    window.popup = window.open('../resources/inspector-protocol-page.html');
    undefined;
  `);
  const popupSession =
      testRunner.createSessionFor((await attachedPromise).params.sessionId);
  const popupDp = popupSession.protocol;
  await popupDp.Page.enable();
  const loadPromise = popupDp.Page.onceLoadEventFired();
  await popupDp.Runtime.runIfWaitingForDebugger();
  await openPromise;
  await loadPromise;
  await popupSession.evaluate(`
    document.body.innerHTML =
        '<input aria-label="popup-field" value="popup-secret">';
    undefined;
  `);

  // Get the popup's input backendNodeId through the popup's own session.
  const popupDoc = (await popupDp.DOM.getDocument()).result.root;
  const popupInputId = (await popupDp.DOM.querySelector({
                         nodeId: popupDoc.nodeId,
                         selector: 'input'
                       })).result.nodeId;
  const foreignId = (await popupDp.DOM.describeNode({
                      nodeId: popupInputId
                    })).result.node.backendNodeId;

  await dp.DOM.enable();
  await dp.Overlay.enable();
  await dp.Accessibility.enable();

  async function backendNodeIdForExpression(expression) {
    const {objectId} = (await dp.Runtime.evaluate({expression})).result.result;
    return (await dp.DOM.describeNode({objectId})).result.node.backendNodeId;
  }

  const doc = (await dp.DOM.getDocument()).result.root;
  const ownInputNodeId =
      (await dp.DOM.querySelector({nodeId: doc.nodeId, selector: 'input'}))
          .result.nodeId;
  const ownInputId = (await dp.DOM.describeNode({
                       nodeId: ownInputNodeId
                     })).result.node.backendNodeId;
  // <template> content lives in a frameless document of the inspected frame.
  const templateSpanId = await backendNodeIdForExpression(
      'document.querySelector("template").content.firstChild');
  const iframeInputId =
      await backendNodeIdForExpression('window.retainedIframeInput');
  const iframeTemplateSpanId =
      await backendNodeIdForExpression('window.retainedIframeTemplateSpan');

  function logResult(name, response) {
    if (response.error) {
      testRunner.log(`${name}: error: ${response.error.message}`);
    } else {
      testRunner.log(`${name}: resolved`);
    }
  }

  testRunner.log('Nodes of the inspected target resolve:');
  for (const [label, backendNodeId] of [
           ['input', ownInputId], ['template content', templateSpanId],
           ['subframe input', iframeInputId],
           ['subframe template content', iframeTemplateSpanId]]) {
    const {result} = await dp.DOM.describeNode({backendNodeId});
    testRunner.log(`DOM.describeNode(${label}): ${result.node.nodeName}`);
  }
  logResult('DOM.resolveNode(input, executionContextId)',
            await dp.DOM.resolveNode(
                {backendNodeId: ownInputId, executionContextId}));
  logResult('Accessibility.getAXNodeAndAncestors(input)',
            await dp.Accessibility.getAXNodeAndAncestors(
                {backendNodeId: ownInputId}));

  // Removing the subframe shuts down its document (clearing execution_context_)
  // and disconnects its LocalDOMWindow from its frame (leaving the subframe's
  // frameless <template> document with a detached LocalDOMWindow).
  await session.evaluate('document.querySelector("iframe").remove()');
  testRunner.log('Nodes of a detached subframe are rejected:');
  logResult('DOM.describeNode(detached subframe input)',
            await dp.DOM.describeNode({backendNodeId: iframeInputId}));
  logResult('DOM.describeNode(detached subframe template content)',
            await dp.DOM.describeNode({backendNodeId: iframeTemplateSpanId}));

  testRunner.log('Nodes of the popup target are rejected:');
  logResult('DOM.describeNode',
            await dp.DOM.describeNode({backendNodeId: foreignId}));
  logResult('DOM.resolveNode',
            await dp.DOM.resolveNode({backendNodeId: foreignId}));
  logResult(
      'DOM.resolveNode(executionContextId)',
      await dp.DOM.resolveNode({backendNodeId: foreignId, executionContextId}));
  logResult('DOM.getBoxModel',
            await dp.DOM.getBoxModel({backendNodeId: foreignId}));
  logResult('Overlay.highlightNode',
            await dp.Overlay.highlightNode(
                {highlightConfig: {}, backendNodeId: foreignId}));
  logResult(
      'Accessibility.getPartialAXTree',
      await dp.Accessibility.getPartialAXTree({backendNodeId: foreignId}));
  logResult(
      'Accessibility.getAXNodeAndAncestors',
      await dp.Accessibility.getAXNodeAndAncestors({backendNodeId: foreignId}));
  logResult('Accessibility.queryAXTree',
            await dp.Accessibility.queryAXTree(
                {backendNodeId: foreignId, accessibleName: 'popup-field'}));

  testRunner.completeTest();
});
