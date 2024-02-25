(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
    <style>
    div {
      color: var(--registered-prop);
    }
    @property --registered-prop {
      inherits: true;
      initial-value: red;
      syntax: "<color>";
    }
    #invalid {
      --unregistered-prop: yes;
      --registered-prop: xyz;
    }
    #initial {
      --unregistered-prop: yes;
      --registered-prop: initial;
    }
    #inherit {
      --unregistered-prop: yes;
      --registered-prop: inherit;
    }
    #unset {
      --unregistered-prop: yes;
      --registered-prop: unset;
    }
    #revert {
      --unregistered-prop: yes;
      --registered-prop: revert;
    }
    #revert-layer {
      --unregistered-prop: yes;
      --registered-prop: revert-layer;
    }
    </style>
    <div id="invalid">Text</div>
    <div id="initial">Text</div>
    <div id="inherit">Text</div>
    <div id="unset">Text</div>
    <div id="revert">Text</div>
    <div id="revert-layer">Text</div>
  `,
      'Test reporting of defaulting values for registered custom properties');

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

  const selectors = ['#invalid', '#initial', '#inherit', '#unset',
      '#revert', '#revert-layer'];

  for (const selector of selectors) {
    const nodeId = await requestNodeId(documentNodeId, selector);
    const {result} = await dp.CSS.getMatchedStylesForNode({nodeId});

    const rules = result.matchedCSSRules.flat().filter(
        rule => rule.rule.selectorList.selectors.some(
            s => s.text === selector));
    const properties =
        rules.map(rule => rule.rule.style.cssProperties
            .filter(prop => prop.range))
            .flat();
    const parsedOk =
        properties.map(({name, parsedOk}) => ({name, parsedOk})).toSorted();
    testRunner.log(parsedOk);

  }

  testRunner.completeTest();
});
