(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink="http://www.w3.org/1999/xlink" version="1.1">
      <image id='main' xlink:href="fake_original_image.png"/>
    </svg>
  `, 'Test that namespaced attributes can be modified.');

  var response = await dp.DOM.getDocument();
  var rootNodeId = response.result.root.nodeId;

  response = await dp.DOM.querySelector({nodeId: rootNodeId, selector: '#main'})
  var nodeId = response.result.nodeId;
  testRunner.log('Original attribute:');
  response = await dp.DOM.getAttributes({nodeId});
  testRunner.log(response.result.attributes[2] + '=' + response.result.attributes[3]);

  dp.DOM.setAttributesAsText({nodeId, name: 'xlink:href', text: 'xlink:href="fake_modified_image.png"'});
  response = await dp.DOM.onceAttributeModified();
  testRunner.log('Modified attribute:');
  testRunner.log(response.params.name + '=' + response.params.value);
  testRunner.completeTest();
})
