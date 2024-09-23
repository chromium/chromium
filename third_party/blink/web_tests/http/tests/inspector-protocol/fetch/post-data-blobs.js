(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {session, dp} = await testRunner.startBlank(
      `Test interception of post data that are streams`);

    await dp.Runtime.enable();
    dp.Runtime.onConsoleAPICalled(event => testRunner.log(event.params.args[0].value));

    await dp.Network.enable();
    await dp.Runtime.enable();
    await dp.Fetch.enable({ patterns: [{ urlPattern: '*' }] });

    const response = session.evaluateAsync(`
      const blob0 = new Blob(["Kubla Khan"]);
      const blob1 = new Blob(["In Xanadu did ", blob0]);
      const blob2 = new Blob([" A stately pleasure-dome decree"]);

      const body = new FormData();
      body.append("blob 1", blob1);
      body.append("blob 2", blob2);
      fetch('${testRunner.url('./resources/post-echo.pl')}', {
        method: 'POST',
        body,
        headers: {"Content-type": "text/plain"}
      }).then(r => r.text())
    `);

    const requestPaused = (await dp.Fetch.onceRequestPaused()).params;
    const request = requestPaused.request;
    request.postData = stabilizeFormBoundary(request.postData);

    request.postDataEntries = request.postDataEntries.map(entry => {
      return {bytes: stabilizeFormBoundary(atob(entry.bytes))};
    });
    testRunner.log(request, undefined, "headers");
    dp.Fetch.continueRequest({requestId: requestPaused.requestId});

    const responseText = stabilizeFormBoundary(await response);
    testRunner.log(`echoed request body: ${responseText}`);

    testRunner.completeTest();

    function stabilizeFormBoundary(str) {
      return str.replace(/(^------WebKitFormBoundary)\S*$/mg, "$1").replace(/\r\n/g, "\n");
    }
  })
