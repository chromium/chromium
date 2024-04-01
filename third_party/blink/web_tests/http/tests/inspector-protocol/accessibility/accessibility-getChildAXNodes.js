(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  function printNodeAndChildren(node, leadingSpace = "") {
    if (node.ignored) {
      return node.children.map((child) => printNodeAndChildren(child, leadingSpace)).join("\n");
    }

    let string = leadingSpace;
    if (node.role)
      string += node.role.value;
    else
      string += "<no role>";
    string += (node.name && node.name.value ? ` "${node.name.value}"` : "");
    for (const child of node.children)
      string += "\n" + printNodeAndChildren(child, leadingSpace + "  ");
    return string;
  }

  var {page, session, dp} = await testRunner.startHTML(`
  <main>
    <article>
      <h1>Article</h1>
      <p>First paragraph</p>
    </article>
    <iframe src="${testRunner.url('../resources/iframe-accessible-name.html')}"></iframe>
  </main>
  `, 'Tests Accessibility.getChildAXNodes');
  await dp.Accessibility.enable();

  let {result} = await dp.Accessibility.getFullAXTree({depth: 2});
  let iframeNode;
  for (const node of result.nodes) {
    if (node.role?.value === 'Iframe') {
      iframeNode = node;
      break;
    }
  }

  const nodeMap = new Map();

  for (const node of result.nodes)
    nodeMap.set(node.nodeId, node);
  for (const [nodeId, node] of nodeMap.entries()) {
    node.children = [];
    for (const childId of node.childIds || []) {
      const child = nodeMap.get(childId);
      if (!child)
        continue;
      child.parent = node;
      node.children.push(child);
    }
  }
  const rootNode = Array.from(nodeMap.values()).find(node => !node.parent);
  testRunner.log("\nAfter getFullAXTree:\n" + printNodeAndChildren(rootNode))

  const article = rootNode.children[0].children[0].children[0].children[0];
  let childResult = await dp.Accessibility.getChildAXNodes({id: article.nodeId});
  result = childResult.result;
  for (const node of result.nodes) {
    nodeMap.set(node.nodeId, node);
    node.children = [];
  }

  for (const childId of article.childIds) {
    const child = nodeMap.get(childId);
    if (!child) {
      testRunner.log("Should have gotten child with id " + childId);
      continue;
    }
    child.parent = article;
    article.children.push(child);
  }

  testRunner.log("\nAfter getChildAXNodes:\n" + printNodeAndChildren(rootNode));

  const iframeDescribeResp = await dp.DOM.describeNode({backendNodeId: iframeNode.backendDOMNodeId});
  const frameId = iframeDescribeResp.result.node.frameId;
  // We retrieve the root node of the child frame using `getFullAXTree` with depth = 0 to omit its children.
  const iframeResult = await dp.Accessibility.getFullAXTree({depth: 0, frameId});
  const rootWebArea = iframeResult.result.nodes;
  // To test that frameIds are handled correctly, we can now fetch the children of the iframe root using
  // `getChildAXNodes` with the frameId of the iframe.
  const genericResult = await dp.Accessibility.getChildAXNodes({id: rootWebArea[0].nodeId, frameId});
  const generic = genericResult.result.nodes;
  const heading = await dp.Accessibility.getChildAXNodes({id: generic[0].childIds[0], frameId});

  iframeNode.childIds = [rootWebArea[0].nodeId];
  for (const node of [...rootWebArea, ...heading.result.nodes])
    nodeMap.set(node.nodeId, node);
  for (const [nodeId, node] of nodeMap.entries()) {
    node.children = [];
    for (const childId of node.childIds || []) {
      const child = nodeMap.get(childId);
      if (!child)
        continue;
      child.parent = node;
      node.children.push(child);
    }
  }

  testRunner.log("\ngetChildAXNodes iframe:\n" + printNodeAndChildren(rootNode));

  testRunner.completeTest();
});
