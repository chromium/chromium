(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL('../resources/dom-snapshot.html', 'Tests DOMSnapshot.getSnapshot method.');

  await session.evaluate(`
    var host = document.querySelector('#shadow-host').attachShadow({mode: 'open'});
    var template = document.querySelector('#shadow-template');
    host.appendChild(template.content);
    template.remove();
    document.body.offsetWidth;
  `);

  function stabilize(key, value) {
    var unstableKeys = ['documentURL', 'baseURL', 'frameId', 'backendNodeId', 'scriptId'];
    if (unstableKeys.indexOf(key) !== -1)
      return '<' + typeof(value) + '>';
    if (typeof value === 'string' && value.indexOf('/dom-snapshot/') !== -1)
      value = '<value>';
    if (typeof value === 'string' && value.indexOf('file://') !== -1)
      value = '<string>' + value.replace(/.*(LayoutTests|web_tests)\//, '');
    return value;
  }

  var whitelist = ['transform', 'transform-origin', 'height', 'width', 'display', 'outline-color', 'color'];
  var response = await dp.DOMSnapshot.getSnapshot({'computedStyleWhitelist': whitelist, 'includeEventListeners': true});
  if (response.error)
    testRunner.log(response);
  else
    testRunner.log(JSON.stringify(response.result, stabilize, 2));
  testRunner.completeTest();
})
