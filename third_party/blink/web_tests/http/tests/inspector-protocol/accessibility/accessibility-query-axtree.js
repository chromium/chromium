// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
      <h2 id="shown">title</h2>
      <h2 id="hidden" aria-hidden="true" lang="x">title</h2>
      <h2 id="unrendered" hidden lang="x">title</h2>

      <div id="node1" aria-labeledby="node2"></div>
      <div id="node2" aria-label="bar"></div>
      <div id="node3" aria-label="foo" aria-hidden="true"></div>
      <div id="node4" class="container">
          <div id="node5" role="button" aria-label="foo"></div>
          <div id="node6" role="button" aria-label="foo"></div>
          <div id="node7" hidden role="button" aria-label="foo"></div>
          <div id="node8" role="button" aria-label="bar"></div>
      </div>

      <button id="node10">text content</button>
      <h1 id="node11">text content</h1>
      <!-- Accessible name not available when role is "presentation" -->
      <h1 id="node12" role="presentation">text content</h1>
      <!-- Elements inside shadow dom should be found -->
      <script>
        const div = document.createElement('div');
        const shadowRoot = div.attachShadow({mode: 'open'});
        const h1 = document.createElement('h1');
        h1.textContent = 'text content';
        h1.id = 'node13';
        shadowRoot.appendChild(h1);
        document.documentElement.appendChild(div);
      </script>

      <img id="node20" src="" alt="Accessible Name">
      <input id="node21" type="submit" value="Accessible Name">
      <label id="node22" for="node23">Accessible Name</label>
      <input id="node23">
      <!-- Accessible name for the <input> is "Accessible Name" -->
      <div id="node24" title="Accessible Name"></div>

      <div role="treeitem" id="node30">
        <div role="treeitem" id="node31">
          <div role="treeitem" id="node32">item1</div>
          <div role="treeitem" id="node33">item2</div>
        </div>
        <div role="treeitem" id="node34">item3</div>
      </div>
      <!-- Accessible name for the following <div> is "item1 item2 item3" -->
      <div aria-describedby="node30"></div>
      <header id="header">role=[banner] test</header>
      <div id="shadow-host">
        <template shadowrootmode="open">
          <input id="shadow-input" placeholder="Shadow input"></input>
        </template>
      </div>
    `,
      'Test finding DOM nodes by accessible name');


  const documentResp = await dp.DOM.getDocument();
  const documentId = documentResp.result.root.nodeId;

  const containerResp =
      await dp.DOM.querySelector({nodeId: documentId, selector: '.container'});
  const containerId = containerResp.result.nodeId;

  const shadowHostResp =
    await dp.DOM.querySelector({nodeId: documentId, selector: '#shadow-host'});
  const shadowHostId = shadowHostResp.result.nodeId;
  const shadowHostDescribeResp = await dp.DOM.describeNode({nodeId: shadowHostId});
  const shadowRootId = shadowHostDescribeResp.result.node.shadowRoots[0].backendNodeId;

  // gymnastics to get remoteObjectIds from nodes
  const documentResp2 = await dp.DOM.resolveNode({nodeId: documentId});
  const documentObjId = documentResp2.result.object.objectId;
  const containerResp2 = await dp.DOM.resolveNode({nodeId: containerId});
  const containerObjId = containerResp2.result.object.objectId;

  async function dumpAXNodes() {
    testRunner.log('dump both an ignored and an unignored axnode');
    const response = await dp.Accessibility.queryAXTree(
        {objectId: documentObjId, accessibleName: 'title'});
    for (const axnode of response.result.nodes) {
      testRunner.log(axnode, null, ['nodeId', 'backendDOMNodeId', 'childIds', 'parentId']);
    }
  }

  async function testGetNodesForSubtreeByAccessibleName() {
    let response;

    testRunner.log('find all elements with accessible name "foo"');
    response = await dp.Accessibility.queryAXTree(
        {objectId: documentObjId, accessibleName: 'foo'});
    await logNodes(response.result.nodes);

    testRunner.log(
        'find all elements with accessible name "foo" inside container');
    response = await dp.Accessibility.queryAXTree(
        {objectId: containerObjId, accessibleName: 'foo'});
    await logNodes(response.result.nodes);

    testRunner.log('find all elements with accessible name "bar"');
    response = await dp.Accessibility.queryAXTree(
        {objectId: documentObjId, accessibleName: 'bar'});
    await logNodes(response.result.nodes);

    testRunner.log('find all elements with accessible name "text content"');
    response = await dp.Accessibility.queryAXTree(
        {objectId: documentObjId, accessibleName: 'text content'});
    await logNodes(response.result.nodes);

    testRunner.log('find all elements with accessible name "Accessible Name"');
    response = await dp.Accessibility.queryAXTree(
        {objectId: documentObjId, accessibleName: 'Accessible Name'});
    await logNodes(response.result.nodes);

    testRunner.log(
      'find all elements with accessible name "item1 item2 item3"');
    response = await dp.Accessibility.queryAXTree(
        {objectId: documentObjId, accessibleName: 'item1 item2 item3'});
    await logNodes(response.result.nodes);
  }

  async function testGetNodesForShadowRoot() {
    testRunner.log(
      'find all elements with accessible name "Shadow input" (expected: 1 node)');
    response = await dp.Accessibility.queryAXTree(
        {backendNodeId: shadowRootId, accessibleName: 'Shadow input'});
    await logNodes(response.result.nodes);
  }

  async function testGetNodesForSubtreeByRole() {
    let response;

    testRunner.log('find all elements with role "button"');
    response = await dp.Accessibility.queryAXTree(
        {objectId: documentObjId, role: 'button'});
    await logNodes(response.result.nodes);

    testRunner.log('find all elements with role "heading"');
    response = await dp.Accessibility.queryAXTree(
        {objectId: documentObjId, role: 'heading'});
    await logNodes(response.result.nodes);

    testRunner.log('find all elements with role "treeitem"');
    response = await dp.Accessibility.queryAXTree(
        {objectId: documentObjId, role: 'treeitem'});
    await logNodes(response.result.nodes);

    testRunner.log('find all ignored nodes with role "presentation"');
    response = await dp.Accessibility.queryAXTree(
        {objectId: documentObjId, role: 'presentation'});
    await logNodes(response.result.nodes);

    testRunner.log('find all nodes with role "banner" (expected: 1 node)');
    response = await dp.Accessibility.queryAXTree(
        {objectId: documentObjId, role: 'banner'});
    await logNodes(response.result.nodes);
  }

  async function testGetNodesForSubtreeByAccessibleNameAndRole() {
    let response;

    testRunner.log(
        'find all elements with accessible name "foo" and role "button"');
    response = await dp.Accessibility.queryAXTree(
        {objectId: documentObjId, accessibleName: 'foo', role: 'button'});
    await logNodes(response.result.nodes);

    testRunner.log(
        'find all elements with accessible name "foo" and role "button" inside container');
    response = await dp.Accessibility.queryAXTree(
        {objectId: containerObjId, accessibleName: 'foo', role: 'button'});
    await logNodes(response.result.nodes);

    testRunner.log(
        'find all elements with accessible name "text content" and role "heading"');
    response = await dp.Accessibility.queryAXTree({
      objectId: documentObjId,
      accessibleName: 'text content',
      role: 'heading'
    });
    await logNodes(response.result.nodes);

    testRunner.log(
        'find all elements with accessible name "text content" and role "button"');
    response = await dp.Accessibility.queryAXTree({
      objectId: documentObjId,
      accessibleName: 'text content',
      role: 'button'
    });
    await logNodes(response.result.nodes);

    testRunner.log(
        'find all elements with accessible name "Accessible Name" and role "textbox"');
    response = await dp.Accessibility.queryAXTree({
      objectId: documentObjId,
      accessibleName: 'Accessible Name',
      role: 'textbox'
    });
    await logNodes(response.result.nodes);

    testRunner.log(
        'find all elements with accessible name "Accessible Name" and role "button"');
    response = await dp.Accessibility.queryAXTree({
      objectId: documentObjId,
      accessibleName: 'Accessible Name',
      role: 'button'
    });
    await logNodes(response.result.nodes);
  }

  async function logNodes(axNodes) {
    for (const axNode of axNodes) {
      const backendNodeId = axNode.backendDOMNodeId;
      const response = await dp.DOM.describeNode({backendNodeId});
      const node = response.result.node;
      // we can only print ids for ELEMENT_NODEs, skip TEXT_NODEs
      if (node.nodeType !== Node.ELEMENT_NODE) {
        continue;
      }
      const nodeAttributes = node.attributes;
      const idIndex = nodeAttributes.indexOf('id') + 1;
      testRunner.log(nodeAttributes[idIndex]);
    }
  }

  testRunner.runTestSuite([
    dumpAXNodes,
    testGetNodesForSubtreeByAccessibleName,
    testGetNodesForSubtreeByRole,
    testGetNodesForSubtreeByAccessibleNameAndRole,
    testGetNodesForShadowRoot,
  ]);
});
