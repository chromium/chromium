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
    <div>Some text in a div, also a <a href='https://www.example.com'>link</a></div>
    <button>Hello Button</button>
  `, 'Tests Accessibility.getRootAXNode');
  const {result} = await dp.Accessibility.getFullAXTree({depth: 2});

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
  const rootNode = result.nodes[0];
  testRunner.log("\n" + printNodeAndChildren(rootNode));

  testRunner.completeTest();

});
