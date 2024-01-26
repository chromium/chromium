(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests interception events are properly attributed to issuing frames.`);

  session.protocol.Network.enable();
  session.protocol.Page.enable();

  var redirectFilenameToFrameId = new Map();
  session.protocol.Network.onRequestIntercepted(event => {
    var filename = event.params.request.url.split('/').pop();
    redirectFilenameToFrameId.set(filename, event.params.frameId);
    session.protocol.Network.continueInterceptedRequest({interceptionId: event.params.interceptionId});
  });
  await session.protocol.Network.setRequestInterception({patterns: [{urlPattern: "*"}]});

  await session.evaluateAsync(`(function() {
    function waitForLoad(loadable) {
      return new Promise(fulfill => {
        loadable.addEventListener('load', fulfill);
      });
    }
    var script = document.createElement('script');
    script.src = '${testRunner.url('./resources/final.js')}';
    document.body.appendChild(script);

    var style = document.createElement('link');
    style.rel = 'stylesheet';
    style.type = 'text/css';
    style.href = '${testRunner.url('./resources/test.css')}';
    document.head.appendChild(style);

    var iframe = document.createElement('iframe');
    iframe.src = '${testRunner.url('./resources/resource-iframe.html')}';
    document.body.appendChild(iframe);
    return Promise.all([waitForLoad(script), waitForLoad(style), waitForLoad(iframe)]);
  })()`);

  var rootFrame = (await session.protocol.Page.getResourceTree()).result.frameTree;
  var frameURLById = new Map();
  addFramesRecursively(rootFrame);
  function addFramesRecursively(root) {
    frameURLById.set(root.frame.id, root.frame.url);
    (root.childFrames || []).forEach(addFramesRecursively);
  }
  for (var filename of Array.from(redirectFilenameToFrameId.keys()).sort()) {
    var frameURL = frameURLById.get(redirectFilenameToFrameId.get(filename));
    testRunner.log(`${filename} requested by ${frameURL}`);
  }
  testRunner.completeTest();
})
