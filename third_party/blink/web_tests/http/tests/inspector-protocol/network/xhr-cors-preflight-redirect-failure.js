(async function(testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'http://localhost:8000/',
      'Verifies that redirected CORS preflight failure must be detectable.');
  await dp.Network.enable();

  let postRequestId = undefined;
  let optionsRequestId = undefined;
  let postErrorText = undefined;
  let optionsErrorText = undefined;

  dp.Network.onRequestWillBeSent(event => {
    if (event.params.request.method === 'POST') {
      postRequestId = event.params.requestId;
    } else if (event.params.request.method === 'OPTIONS') {
      optionsRequestId = event.params.requestId;
    }
  });

  dp.Network.onLoadingFailed(async event => {
    if (postRequestId === event.params.requestId) {
      postErrorText = event.params.errorText;
    } else if (optionsRequestId === event.params.requestId) {
      optionsErrorText = event.params.errorText;
    }

    if (postErrorText !== undefined && optionsErrorText !== undefined) {
      testRunner.log(`error text for OPTIONS request: ${optionsErrorText}`);
      testRunner.log(`error text for POST request: ${postErrorText}`);
      testRunner.completeTest();
    }
  });

  await session.evaluate(`
      const xhr = new XMLHttpRequest();
      xhr.open('POST', 'http://127.0.0.1:8000/inspector-protocol/network/resources/cors-preflight-redirect.php');
      xhr.setRequestHeader('header-name', 'header-value');
      xhr.send(JSON.stringify({data: 'test post data'}));
  `);
})
