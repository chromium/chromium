(async function (testRunner) {

  const { dp, session, page } = await testRunner.startHTML(`
    <!DOCTYPE html>
    <html>
    <head>
      <title>Accessibility tree observer test</title>

      <script>
        let childNo = 1;
        function addDivChild() {
          let target = document.getElementById('childrenAddRemoveTarget');
          let para = document.createElement('p');
          let text = document.createTextNode('ChildAdded');
          para.appendChild(text);
          target.appendChild(para);
          childNo++;
        }

        function modifyData() {
          let target = document.getElementById('editDiv').firstChild;
          target.appendData('AddedText.')
        }
      </script>
    </head>

    <body>
      <div id="editDiv" aria-label="editDiv" contenteditable>InitialText.</div>
      <div id="childrenAddRemoveTarget" aria-label="childrenAddRemoveTarget"></div>
    </body>
    </html>
`, 'Test that accessibility tree changes triggers events');

  function logNode(node) {
    testRunner.log(node, null, ['childIds', 'frameId', 'parentId', 'backendDOMNodeId']);
  }

  async function fetchTree() {
    await dp.Accessibility.enable();
    const rootMessage = await dp.Accessibility.getRootAXNode({});
    const result = [];
    const queue = [rootMessage.result.node];
    while (queue.length) {
      const node = queue.pop();
      result.push(node);
      const children = await dp.Accessibility.getChildAXNodes({id: node.nodeId});
      queue.push(...children.result.nodes);
    }
    return result;
  }

  async function expectEventFiresForAppendedNodes() {
    const tree = await fetchTree();
    const targetNode = tree.find(node => node.name?.value === 'childrenAddRemoveTarget');
    logNode(targetNode);

    session.evaluate('addDivChild()');
    const nodesUpdatedMessage = await dp.Accessibility.onceNodesUpdated();
    logNode(nodesUpdatedMessage.params);
    testRunner.log(`Nodes updated node ID is equal to childrenAddRemoveTarget ID? ${nodesUpdatedMessage.params.nodes[0].nodeId === targetNode.nodeId}`);
  }

  async function expectEventFiredWhenDataModified() {
    const tree = await fetchTree();
    const targetNode = tree.find(node => node.name?.value === 'InitialText.');
    const targetNodeId = targetNode.nodeId;
    if (!targetNodeId)
      testRunner.fail('Unable to get target node Id for comparison.');

    session.evaluate('modifyData()');
    const nodesUpdatedMessage = await dp.Accessibility.onceNodesUpdated();
    logNode(nodesUpdatedMessage.params);
    testRunner.log(`Received update for textnode? ${nodesUpdatedMessage.params.nodes[0].nodeId === targetNodeId}`);
  }

  testRunner.runTestSuite([
    expectEventFiresForAppendedNodes,
    expectEventFiredWhenDataModified,
  ]);

});
