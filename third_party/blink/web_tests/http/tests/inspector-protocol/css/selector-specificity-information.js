(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <style>
      body {
        color: red;
      }
      .class {
        color: red;
      }
      #id {
        color: red;
      }
    </style>
    <body>
      <span class="class"></span>
      <span id="id"></span>
    </body>
  `, 'Test that specificity information is passed to DevTools.');

  await dp.DOM.enable();
  await dp.CSS.enable();

  async function requestDocumentNodeId() {
    const {result} = await dp.DOM.getDocument({});
    return result.root.nodeId;
  }
  async function requestNodeId(nodeId, selector) {
    const {result} = await dp.DOM.querySelector({nodeId, selector});
    return result.nodeId;
  }

  const documentNodeId = await requestDocumentNodeId();

  for (const selector of ['body', '.class', '#id']) {
    const nodeId = await requestNodeId(documentNodeId, selector);
    const { result: matchedStylesForNode } = await dp.CSS.getMatchedStylesForNode({ nodeId });

    const specificity = matchedStylesForNode.matchedCSSRules[0].rule.selectorList.selectors[0].specificity;
    testRunner.log(`${selector}: ${specificity.a},${specificity.b},${specificity.c}`);
  }

  testRunner.completeTest();
});

