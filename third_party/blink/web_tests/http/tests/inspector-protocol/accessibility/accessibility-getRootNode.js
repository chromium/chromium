(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp, page} = await testRunner.startBlank('Tests Accessibility.getRootAXNode');

  await dp.Accessibility.enable();

  const complete = dp.Accessibility.onceLoadComplete();

  await page.navigate(testRunner.url('../resources/page-with-iframe-accessible-name.html'));

  function logNode(axnode) {
    testRunner.log(axnode, null, ['nodeId', 'backendDOMNodeId', 'childIds', 'frameId', 'parentId', 'properties']);
  }

  await complete;

  let {result} = await dp.Accessibility.getFullAXTree({depth: 2});
  let iframeNode;
  for (const node of result.nodes) {
    if (node.role?.value === 'Iframe') {
      iframeNode = node;
      break;
    }
  }

  const rootResult = await dp.Accessibility.getRootAXNode({});
  testRunner.log("\ngetRootAXNode for main frame:\n");
  logNode(rootResult.result.node);

  const iframeDescribeResp = await dp.DOM.describeNode({backendNodeId: iframeNode.backendDOMNodeId});
  const frameId = iframeDescribeResp.result.node.frameId;
  const iframeResult = await dp.Accessibility.getRootAXNode({frameId});

  testRunner.log("\ngetRootAXNode for iframe:\n");
  logNode(iframeResult.result.node);

  testRunner.completeTest();
});
