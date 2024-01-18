(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Test frame attirbution of requests from about:blank frames`);

  await Promise.all([
    dp.Runtime.enable(),
    dp.Fetch.enable()
  ]);

  dp.Fetch.onRequestPaused(event => {
    dp.Fetch.continueRequest({requestId: event.params.requestId})
  });
  const appendStylesheet = function(doc, href) {
    const stylesheet = doc.createElement('link');
    stylesheet.href = href;
    stylesheet.rel = 'stylesheet';
    stylesheet.type = 'text/css';
    doc.head.appendChild(stylesheet);
  }
  session.evaluate(`
    (${appendStylesheet.toString()})(document, '${testRunner.url('../network/resources/test.css')}')`);

  const topFrameRequest = (await dp.Fetch.onceRequestPaused()).params;
  dp.Fetch.continueRequest({requestId: topFrameRequest.requestId});

  async function makeRequestInFrame(url) {
    // Load frame first, so we don't intercept request associated with it being loaded.
    await session.evaluateAsync(`(function() {
      const iframe = document.createElement('iframe');
      iframe.src = '${url}';
      const loadPromise = new Promise(resolve => iframe.addEventListener('load', resolve));
      document.body.appendChild(iframe);
      window.lastFrame = iframe;
      return loadPromise;
    })()`);
    session.evaluate(`
      (${appendStylesheet.toString()})(window.lastFrame.contentDocument,
        '${testRunner.url('../network/resources/test.css')}?url=${url}');
    `);
    return (await dp.Fetch.onceRequestPaused()).params;
  }

  const blankFrameRequest = await makeRequestInFrame('about:blank');
  const httpFrameRequest = await makeRequestInFrame(testRunner.url('../network/resources/simple-iframe.html'));

  const {frameTree} = (await dp.Page.getFrameTree()).result;

  const frameMap = new Map();
  function traverseFrameTree(root) {
    frameMap.set(root.frame.id, root.frame.url);
    for (const f of root.childFrames || [])
      traverseFrameTree(f);
  }
  traverseFrameTree(frameTree);

  testRunner.log(`${topFrameRequest.request.url} requested by ${frameMap.get(topFrameRequest.frameId)}`);
  testRunner.log(`${blankFrameRequest.request.url} requested by ${frameMap.get(blankFrameRequest.frameId)}`);
  testRunner.log(`${httpFrameRequest.request.url} requested by ${frameMap.get(httpFrameRequest.frameId)}`);

  testRunner.completeTest();
})
