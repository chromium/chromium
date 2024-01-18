(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
  <main>
    <article>
      <h1>Article</h1>
      <p>First paragraph</p>
    </article>
  </main>
  `, 'Tests Accessibility.getAXNodeAndAncestors');
  await dp.Accessibility.enable();

  function logNode(axnode) {
    testRunner.log(axnode, null, ['nodeId', 'backendDOMNodeId', 'childIds', 'frameId', 'parentId']);
  }

  const documentResp = await dp.DOM.getDocument();
  const documentId = documentResp.result.root.nodeId;
  const paragraphResp =
    await dp.DOM.querySelector({nodeId: documentId, selector: 'p'});
  const paragraphId = paragraphResp.result.nodeId;

  let {result} = await dp.Accessibility.getAXNodeAndAncestors({nodeId: paragraphId});
  for (const node of result.nodes) {
    logNode(node);
  }

  testRunner.completeTest();
});
