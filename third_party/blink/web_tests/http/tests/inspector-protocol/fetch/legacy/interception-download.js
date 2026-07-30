(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that downloads are correctly identified when intercepting navigation responses.`);

  await session.protocol.Network.clearBrowserCache();
  await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
  await session.protocol.Network.enable();
  await session.protocol.Runtime.enable();

  await dp.Fetch.enable(
      {patterns: [{urlPattern: '*', requestStage: 'Response'}]});


  testRunner.log('Regular navigation: ');
  dp.Page.navigate(
      {url: 'http://127.0.0.1:8000/devtools/network/resources/resource.php'});
  let {params} = await dp.Fetch.onceRequestPaused();
  testRunner.log(`Intercepted ${params.request.url}`);
  dp.Fetch.continueRequest({requestId: params.requestId});

  testRunner.log('Download via content-disposition: ');
  dp.Page.navigate({
    url:
        'http://127.0.0.1:8000/devtools/network/resources/resource.php?download=1'
  });
  ({params} = await dp.Fetch.onceRequestPaused());
  testRunner.log(`Intercepted ${params.request.url}`);
  dp.Fetch.continueRequest({requestId: params.requestId});

  testRunner.log('Download via unhandled MIME type: ');
  dp.Page.navigate({
    url:
        'http://127.0.0.1:8000/devtools/network/resources/resource.php?mime_type=application/octet-stream'
  });
  ({params} = await dp.Fetch.onceRequestPaused());
  testRunner.log(`Intercepted ${params.request.url}`);
  dp.Fetch.continueRequest({requestId: params.requestId});

  testRunner.log('Now downloading by clicking a link: ');
  session.evaluate(`
    const a = document.createElement('a');
    a.href = '/devtools/network/resources/resource.php';
    a.download = 'hello.text';
    document.body.appendChild(a);
    a.click();
  `);
  ({params} = await dp.Fetch.onceRequestPaused());
  testRunner.log(`Intercepted ${params.request.url}`);
  dp.Fetch.continueRequest({requestId: params.requestId});

  testRunner.completeTest();
})
