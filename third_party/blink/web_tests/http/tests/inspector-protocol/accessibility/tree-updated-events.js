(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {

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

    session.evaluate('addDivChild()');
    const nodesUpdatedMessage = await dp.Accessibility.onceNodesUpdated();
    const targetNoddeUpdate = nodesUpdatedMessage.params.nodes.find(node => node.nodeId === targetNode.nodeId);
    testRunner.log(`Nodes updated includes node ID equal to childrenAddRemoveTarget ID? ${Boolean(targetNoddeUpdate)}`);
  }

  async function expectEventFiredWhenDataModified() {
    const tree = await fetchTree();
    const targetNode = tree.find(node => node.name?.value === 'InitialText.');
    const targetNodeId = targetNode.nodeId;
    if (!targetNodeId)
      testRunner.fail('Unable to get target node Id for comparison.');

    session.evaluate('modifyData()');
    const nodesUpdatedMessage = await dp.Accessibility.onceNodesUpdated();
    const targetNoddeUpdate = nodesUpdatedMessage.params.nodes.find(node => node.nodeId === targetNodeId);
    testRunner.log(`Received update for textnode? ${Boolean(targetNoddeUpdate)}`);
  }

  testRunner.runTestSuite([
    expectEventFiresForAppendedNodes,
    expectEventFiredWhenDataModified,
  ]);

});
