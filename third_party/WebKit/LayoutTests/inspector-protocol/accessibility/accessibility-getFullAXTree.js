(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div>Some text in a div, also a <a href='https://www.example.com'>link</a></div>
    <button>Hello Button</button>
  `, 'Tests Accessibility.getFullAXTree');
  const {result} = await dp.Accessibility.getFullAXTree();
  printNodes(result.nodes);

  testRunner.completeTest();

  function printNodes(nodes) {
    function printNodeAndChildren(node, leadingSpace = "") {
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

    const nodeMap = new Map();
    for (const node of nodes)
      nodeMap.set(node.nodeId, node);
    for (const [nodeId, node] of nodeMap.entries()) {
      node.children = [];
      for (const childId of node.childIds || []) {
        const child = nodeMap.get(childId);
        child.parent = node;
        node.children.push(child);
      }
    }
    const rootNode = Array.from(nodeMap.values()).find(node => !node.parent);
    testRunner.log("\n" + printNodeAndChildren(rootNode));
  }
});
