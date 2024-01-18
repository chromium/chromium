(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <style>
      #outer::selection {
        color: limegreen;
      }

      #middle::highlight(foo) {
        color: red;
      }

      #middle::highlight(bar) {
        color: orange;
      }

      #target::highlight(baz) {
        color: lightblue;
      }

      body::first-letter {
        color: yellow;
      }
    </style>
    <body>
      <div id="outer">
        <div>
          <div id="middle">
            <span id="target">target</span>
          </div>
        </div>
      </div>
    </body>
  `, 'Test that inherited highlight pseudos are reported in getMatchedStylesForNode.');

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

  function logPseudoElementMatches(pseudoElementMatches) {
    testRunner.log(`Found ${pseudoElementMatches.length} pseudo element matches`);
    for (let i = 0; i < pseudoElementMatches.length; i++) {
      testRunner.log(`Match ${i} has pseudoType ${pseudoElementMatches[i].pseudoType}`);
      testRunner.log(`Match ${i} has ${pseudoElementMatches[i].matches.length} matched rules`);
      for (let j = 0; j < pseudoElementMatches[i].matches.length; j++) {
        testRunner.log(`Match ${i}'s rule ${j} has cssText ${pseudoElementMatches[i].matches[j].rule.style.cssText.trim()}`);
      }
    }
  }

  const { result: matchedStylesForNode } = await dp.CSS.getMatchedStylesForNode({ nodeId });

  testRunner.log("Logging #target's own pseudos:");
  logPseudoElementMatches(matchedStylesForNode.pseudoElements);

  const inheritedPseudos = matchedStylesForNode.inheritedPseudoElements;
  testRunner.log("Logging #target's inherited pseudos:")
  for(let i = 0; i < inheritedPseudos.length; i++) {
    logPseudoElementMatches(inheritedPseudos[i].pseudoElements);
  }

  testRunner.completeTest();
});

