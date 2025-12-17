(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that Network.requestWillBeSent reports render blocking status.');

  await dp.Network.enable();

  const requests = new Map();
  const numRequests = 8;
  const requestsPromise = new Promise(resolve => {
    dp.Network.onRequestWillBeSent(event => {
      const url = event.params.request.url;
      if (!url.includes('?'))
        return;
      const resourceName = url.substring(url.lastIndexOf('?') + 1);
      if (!resourceName || !url.includes('empty'))
        return;
      requests.set(resourceName, event.params.renderBlockingBehavior);
      if (requests.size === numRequests)
        resolve();
    });
  });

  await page.navigate(testRunner.url('../resources/empty.html'));
  const frameId = (await dp.Page.getFrameTree()).result.frameTree.frame.id;
  await dp.Page.setDocumentContent({
    frameId,
    html: `
    <!DOCTYPE html>
    <html>
    <head>
      <script src="../resources/empty.js?default-blocking"></script>
      <script src="../resources/empty.js?async" async></script>
      <script src="../resources/empty.js?defer" defer></script>
      <link rel="stylesheet" href="../resources/empty.css?default-blocking-css">
      <link rel="stylesheet" media="print" href="../resources/empty.css?non-blocking-media">
    </head>
    <body>
    </body>
    </html>
  `
  });

  await session.evaluateAsync(
      async (url1, styleUrl1, styleUrl2) => {
        const script1 = document.createElement('script');
        script1.src = url1;
        document.head.appendChild(script1);

        const link1 = document.createElement('link');
        link1.rel = 'stylesheet';
        link1.href = styleUrl1;
        document.head.appendChild(link1);

        const link2 = document.createElement('link');
        link2.rel = 'stylesheet';
        link2.href = styleUrl2;
        link2.blocking = 'render';
        document.head.appendChild(link2);

        // Wait for resources to load to avoid races.
        await Promise.all([
          new Promise(r => script1.onload = r),
          new Promise(r => link1.onload = r),
          new Promise(r => link2.onload = r),
        ]);
      },
      '../resources/empty.js?dynamic', '../resources/empty.css?dynamic-style',
      '../resources/empty.css?dynamic-style-blocking');

  await requestsPromise;

  const sortedNames = [...requests.keys()].sort();
  for (const name of sortedNames) {
    testRunner.log(`${name}: ${requests.get(name) || 'unset'}`);
  }

  testRunner.completeTest();
})
