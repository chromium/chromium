(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <link rel="stylesheet" href="${testRunner.url('../resources/css-redirect.php')}">
    <div id="target">
      Text
    </div>
  `, 'Test disabling of style rules for external CSS files served via a redirect.');

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
  const nodeId = await requestNodeId(documentNodeId, '#target');

  async function getMatchedRule() {
    const { result } = await dp.CSS.getMatchedStylesForNode({ nodeId });
    const matchedRule = result.matchedCSSRules.find(match => match.rule.selectorList.text === '#target');
    if (!matchedRule) {
      return 'default';
    }
    return matchedRule;
  }

  let rule = await getMatchedRule();
  testRunner.log("CSS text for #target before disabling a style:");
  testRunner.log(rule.rule.style.cssText.trim());

  const styleSheetId = rule.rule.style.styleSheetId;

  const response = await dp.CSS.setStyleTexts({
    edits: [
      {
        "styleSheetId":styleSheetId,
        "range": {
          "startLine": 0,
          "startColumn": 9,
          "endLine": 2,
          "endColumn": 0
        },
        "text":"\n\t/* color: blue; */\n"
      }
    ]
  });

  rule = await getMatchedRule();
  testRunner.log("CSS text for #target after disabling a style:");
  testRunner.log(rule.rule.style.cssText.trim());

  testRunner.completeTest();
});

