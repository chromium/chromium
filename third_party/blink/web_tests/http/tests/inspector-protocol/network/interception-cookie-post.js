(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that same site cookies are included with POST request when interception is enabled.`);

  await session.protocol.Network.clearBrowserCache();
  await session.protocol.Network.clearBrowserCookies();
  await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
  await session.protocol.Network.enable();
  await session.protocol.Runtime.enable();

  await dp.Network.setRequestInterception({patterns: [{}]});
  dp.Network.onRequestIntercepted(e => {
    dp.Network.continueInterceptedRequest({interceptionId: e.params.interceptionId});
  });

  dp.Page.navigate({url: 'http://127.0.0.1:8000/inspector-protocol/network/resources/cookie-same-site.pl'});
  await dp.Network.onceLoadingFinished();

  const cookie = await session.evaluate(`
    let form = document.createElement('form');
    form.action = '/inspector-protocol/network/resources/echo-headers.php?headers=HTTP_COOKIE';
    form.method = 'POST';
    let data = document.createElement('input');
    data.type = 'hidden';
    data.name = 'data';
    data.value = 1;
    form.appendChild(data);

    let submit = document.createElement('input');
    submit.type = 'submit';
    form.appendChild(submit);
    document.body.appendChild(form);

    submit.click();
  `);

  const requestId = (await dp.Network.onceLoadingFinished()).params.requestId;
  const content = (await dp.Network.getResponseBody({requestId: requestId})).result.body;
  testRunner.log(`content: ${content}`);
  testRunner.completeTest();
})
