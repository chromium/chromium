(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
    <style>
    :root {
      --registered-prop: 4px;
      --correct-prop: 8px;
    }
    div {
      color: var(--registered-prop);
    }
    @property --registered-prop {
      inherits: false;
      initial-value: red;
      syntax: "<length>";
    }
    @property --registered-prop {
      inherits: false;
      initial-value: red;
      syntax: "<color>";
    }
    </style>
    <script>
    CSS.registerProperty({
      name: '--js-prop',
      inherits: false,
      initialValue: 'red',
      syntax: '<color>',
    });
    </script>
    <div id="target">
      Text
    </div>
  `,
      'Test reporting of parser errors for registered custom properties');

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

  const {result} = await dp.CSS.getMatchedStylesForNode({nodeId});
  const chain = [
    ...result.inherited.map(inherited => inherited.matchedCSSRules),
    result.matchedCSSRules
  ].flat();
  const root = chain.filter(
      rule => rule.rule.selectorList.selectors.some(
          selector => selector.text === ':root'));
  const properties =
      root.map(rule => rule.rule.style.cssProperties.filter(prop => prop.range))
          .flat();
  const parsedOk =
      properties.map(({name, parsedOk}) => ({name, parsedOk})).toSorted();
  testRunner.log(parsedOk);

  testRunner.log(result.cssPropertyRegistrations);
  testRunner.log(result.cssPropertyRules);

  testRunner.completeTest();
});
