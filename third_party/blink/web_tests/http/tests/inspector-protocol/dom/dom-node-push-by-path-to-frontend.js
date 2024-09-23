(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { page, session, dp } = await testRunner.startHTML(`<div id=outer>
  <iframe srcdoc='<html><head><title></title></head><body><div framed=1>Hello world.</div></body></html>'></iframe>
  <p>paragraph</p>
  </div>`, 'Tests that DOM.pushNodeByPathToFrontend() works, including inside iframes');

  await dp.DOM.enable();
  await dp.DOM.getDocument({ depth: -1, pierce: true });

  async function getNode(path) {
    const nodeId =
      (await dp.DOM.pushNodeByPathToFrontend({ path })).result.nodeId;
    return (await dp.DOM.describeNode({ nodeId })).result.node;
  }

  let node;
  node = await getNode('0,HTML,1,BODY')
  testRunner.log(`localName for body: ${node.localName}`);

  node = await getNode('0,HTML,1,BODY,0,DIV,1,P');
  testRunner.log(`localName for p: ${node.localName}`);

  node = await getNode('0,HTML,1,BODY,0,DIV,0,IFRAME');
  testRunner.log(`localName for iframe: ${node.localName}`);

  node = await getNode(
    '0,HTML,1,BODY,0,DIV,0,IFRAME,d,#document,0,HTML,1,BODY,0,DIV');
  testRunner.log(`localName for div inside iframe: ${node.localName}`);
  testRunner.log(`attributes for div inside iframe: ${node.attributes}`);

  testRunner.completeTest();
})
