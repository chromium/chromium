(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests ad-tagging on arbitrary node elements via DOM.adRelatedStateUpdated and DOM.Node.isAdRelated\n`);

  await dp.DOM.enable();
  const {result: docResult} = await dp.DOM.getDocument();

  // Set up the subresource filter to mark 'ad-image.png' as ad-related.
  session.evaluate(`
    testRunner.setDisallowedSubresourcePathSuffixes(["ad-image.png"], false /* block_subresources */);
  `);

  const adImageUrl = testRunner.url('resources/ad-image.png');

  testRunner.log('--- Testing DOM.adRelatedStateUpdated Event Dispatch ---');

  // Create a target node without loading an ad image, ensuring it is not yet ad-tagged.
  session.evaluate(`
    const dynamicSpan = document.createElement('span');
    dynamicSpan.id = 'dynamic-ad-span';
    dynamicSpan.style.display = 'block';
    dynamicSpan.style.width = '100px';
    dynamicSpan.style.height = '100px';
    document.body.appendChild(dynamicSpan);
  `);

  // Map the node in DevTools so the backend assigns it a BoundNodeId.
  // This is required for the backend to dispatch the dynamic event.
  const queryResult1 = await dp.DOM.querySelector({
    nodeId: docResult.root.nodeId,
    selector: '#dynamic-ad-span'
  });
  const dynamicNodeId = queryResult1.result.nodeId;


  // Load a background image into the span. This triggers the ad-tagging.
  session.evaluate(`
    const span = document.getElementById('dynamic-ad-span');
    span.style.backgroundImage = "url('${adImageUrl}')";
  `);

  // Await the event and verify the payload.
  const event = await dp.DOM.onceAdRelatedStateUpdated();
  testRunner.log('Event Triggered: DOM.adRelatedStateUpdated');
  testRunner.log(`Event nodeId matches target: ${event.params.nodeId === dynamicNodeId}`);
  testRunner.log(`Event isAdRelated: ${event.params.adProvenance !== undefined}`);
  if (event.params.adProvenance) {
    testRunner.log(`Event filterlistRule contains 'ad-image.png': ${event.params.adProvenance.filterlistRule.includes('ad-image.png')}`);
  }

  testRunner.log('\n--- Testing Static Serialization ---');

  // Create another node and trigger the ad-related state *before* mapping it in DevTools.
  await session.evaluateAsync(`
    new Promise(resolve => {
      const staticSpan = document.createElement('span');
      staticSpan.id = 'static-ad-span';
      staticSpan.style.display = 'block';
      staticSpan.style.width = '100px';
      staticSpan.style.height = '100px';
      document.body.appendChild(staticSpan);

      // Force the network request and wait for the subresource filter to process it.
      const img = new Image();
      img.onload = img.onerror = () => {
        staticSpan.style.backgroundImage = "url('${adImageUrl}')";
        // Give the layout engine a frame to process the style change and flag the node
        requestAnimationFrame(() => requestAnimationFrame(resolve));
      };
      img.src = "${adImageUrl}";
    })
  `);

  // Map the flagged node in DevTools for the first time.
  const queryResult2 = await dp.DOM.querySelector({
    nodeId: docResult.root.nodeId,
    selector: '#static-ad-span'
  });
  const staticNodeId = queryResult2.result.nodeId;

  // Verify that the initial serialization successfully includes the boolean flag.
  const nodeInfo = await dp.DOM.describeNode({nodeId: staticNodeId});
  testRunner.log(`Node localName: ${nodeInfo.result.node.localName}`);
  testRunner.log(`Node isAdRelated (via describeNode): ${nodeInfo.result.node.adProvenance !== undefined}`);
  if (nodeInfo.result.node.adProvenance) {
    testRunner.log(`Node filterlistRule contains 'ad-image.png': ${nodeInfo.result.node.adProvenance.filterlistRule.includes('ad-image.png')}`);
  }

  testRunner.log('\n--- Testing Script Ancestry ---');

  // Mark 'insert-image.js' as an ad.
  session.evaluate(`
    testRunner.setDisallowedSubresourcePathSuffixes(["insert-image.js"], false /* block_subresources */);
  `);

  const scriptUrl = testRunner.url('resources/insert-image.js');

  // Dynamically append the ad script.
  await session.evaluateAsync(`
    new Promise(resolve => {
      const script = document.createElement('script');
      script.src = "${scriptUrl}";
      script.onload = () => {
        const img = document.getElementById('ad-script-inserted-img');
        if (img.complete) {
          // The image was cached and loaded instantly.
          resolve();
        } else {
          // The image is still fetching over the network.
          img.onload = img.onerror = resolve;
        }
      };
      document.body.appendChild(script);
    });
  `);

  // Map the script-inserted image node in DevTools.
  const queryResult3 = await dp.DOM.querySelector({
    nodeId: docResult.root.nodeId,
    selector: '#ad-script-inserted-img'
  });
  const scriptInsertedNodeId = queryResult3.result.nodeId;

  // Verify provenance via describeNode.
  const scriptNodeInfo = await dp.DOM.describeNode({nodeId: scriptInsertedNodeId});
  testRunner.log(`Node localName: ${scriptNodeInfo.result.node.localName}`);
  testRunner.log(`Node isAdRelated: ${scriptNodeInfo.result.node.adProvenance !== undefined}`);
  if (scriptNodeInfo.result.node.adProvenance) {
    const provenance = scriptNodeInfo.result.node.adProvenance;
    testRunner.log(`Has filterlistRule: ${provenance.filterlistRule !== undefined}`);
    testRunner.log(`Has adScriptAncestry: ${provenance.adScriptAncestry !== undefined}`);
    if (provenance.adScriptAncestry) {
      testRunner.log(`Ancestry chain length > 0: ${provenance.adScriptAncestry.ancestryChain.length > 0}`);
      testRunner.log(`rootScriptFilterlistRule contains 'insert-image.js': ${provenance.adScriptAncestry.rootScriptFilterlistRule.includes('insert-image.js')}`);
    }
  }

  testRunner.completeTest();
})
