(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
  <main>
    <article>
      <h1>Article</h1>
      <p>First paragraph</p>
    </article>
    <iframe src="${testRunner.url('../resources/iframe-accessible-name.html')}"></iframe>
  </main>
  `, 'Tests Accessibility.getRootAXNode');
  await dp.Accessibility.enable();

  function logNode(axnode) {
    testRunner.log(axnode, null, ['nodeId', 'backendDOMNodeId', 'childIds', 'frameId', 'parentId', 'properties']);
  }

  await session.evaluateAsync(() => {
    const iframe = document.querySelector('iframe');
    if (iframe.contentWindow.document.readyState == 'complete') {
      return;
    }
    return new Promise(resolve => {
      iframe.contentWindow.onload = () => {
        resolve();
      }
    });
  });

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
