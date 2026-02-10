(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      testRunner.url('../resources/carousel.html'),
      'Tests that child ax nodes can be retrieved for a node that has been updated in a nodesUpdated event');
  await dp.Accessibility.enable();
  await dp.DOM.enable();
  // Get the root of the document.
  const {result: {root}} = await dp.DOM.getDocument({depth: -1});
  function findNodeByTitle(node, title) {
    if (node.attributes) {
      for (let i = 0; i < node.attributes.length; i += 2) {
        if (node.attributes[i] === 'title' && node.attributes[i+1] === title) {
          return node;
        }
      }
    }
    for (const child of node.children || []) {
      const found = findNodeByTitle(child, title);
      if (found) {
        return found;
      }
    }
    return null;
  }
  const introductionSlide = findNodeByTitle(root, 'Introduction');
  // Use getAXNodeAndAncestors to make sure we get events.
  await dp.Accessibility.getAXNodeAndAncestors({nodeId: introductionSlide.nodeId});
  const {result} = await dp.Accessibility.getFullAXTree();
  const tablist = result.nodes.find(node => node.role?.value === 'tablist');
  const tabIndex = 1;
  const nodesUpdatedPromise = dp.Accessibility.onceNodesUpdated();
  const slidesElementBackendId =
      tablist.properties.find(prop => prop.name === 'controls')
          .value.relatedNodes[0]
          .backendDOMNodeId;
  const slidesElementObjectId =
      (await dp.DOM.resolveNode({backendNodeId: slidesElementBackendId}))
          .result.object.objectId;
  await dp.Runtime.callFunctionOn({
    objectId: slidesElementObjectId,
    functionDeclaration: `function() {
      const slideWidth = this.scrollWidth / ${tablist.childIds.length};
      this.scrollLeft = slideWidth * ${tabIndex};
    }`
  });
  const {nodes: updatedNodes} = (await nodesUpdatedPromise).params;
  const stabilizeIds = ['nodeId', 'backendDOMNodeId', 'objectId', 'backendNodeId', 'id', 'childIds'];
  testRunner.log(updatedNodes, 'nodesUpdated', TestRunner.stabilizeNames, stabilizeIds);
  for (const node of updatedNodes) {
    const childResult = await dp.Accessibility.getChildAXNodes({id: node.nodeId});
    testRunner.log(childResult.result.nodes, 'childNodes', TestRunner.stabilizeNames, stabilizeIds);
  }
  testRunner.completeTest();
});
