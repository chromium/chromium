(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test to make sure if an xhr is fetched with the response as a blob and cross origin devtools can get body.`);

  // This url should be cross origin.
  const url = 'https://127.0.0.1:8443/inspector-protocol/resources/cors-data.php';

  await dp.Network.enable();
  testRunner.log('Network Enabled');

  let getRequestEventParams;
  let optionsRequestEventParams;
  dp.Network.onRequestWillBeSent(event => {
    if (event.params.request.method === 'GET') {
      getRequestEventParams = event.params;
    } else if (event.params.request.method === 'OPTIONS') {
      optionsRequestEventParams = event.params;
      optionsRequestInitiatorRequestId = event.params.initiator.requestId;
    }
  });

  let getRequestExtra = false;
  let optionsRequestExtra = false;
  dp.Network.onRequestWillBeSentExtraInfo(event => {
    if (event.params.requestId === getRequestEventParams.requestId) {
      getRequestExtra = true;
    } else if (event.params.requestId === optionsRequestEventParams.requestId) {
      optionsRequestExtra = true;
    }
  });

  let getResponseExtra = false;
  let optionsResponseExtra = false;
  dp.Network.onResponseReceivedExtraInfo(event => {
    if (event.params.requestId === getRequestEventParams.requestId) {
      getResponseExtra = true;
    } else if (event.params.requestId === optionsRequestEventParams.requestId) {
      optionsResponseExtra = true;
    }
  });

  let getResponseEventParams;
  let optionsResponseEventParams;
  dp.Network.onResponseReceived(event => {
    if (event.params.requestId === getRequestEventParams.requestId) {
      getResponseEventParams = event.params;
    } else if (event.params.requestId === optionsRequestEventParams.requestId) {
      optionsResponseEventParams = event.params;
    }

    if (getResponseEventParams && optionsResponseEventParams) {
      printResultsAndFinish();
    }
  });

  async function printResultsAndFinish() {
    const optionsRequestReferencedGetRequest =
        optionsRequestInitiatorRequestId === getRequestEventParams.requestId;
    testRunner.log('GET Request:');
    testRunner.log(`  Method: ${getRequestEventParams.request.method}`);
    testRunner.log(`  Url: ${getRequestEventParams.request.url}`);
    testRunner.log(`  Has extra info: ${getRequestExtra}`);

    testRunner.log('OPTIONS Request:');
    testRunner.log(`  Method: ${optionsRequestEventParams.request.method}`);
    testRunner.log(`  Url: ${optionsRequestEventParams.request.url}`);
    testRunner.log(`  Has extra info: ${optionsRequestExtra}`);
    testRunner.log(
        `  References get request: ${optionsRequestReferencedGetRequest}`);
    testRunner.log(
        `  Initiator type: ${optionsRequestEventParams.initiator.type}`);

    testRunner.log('GET response:');
    testRunner.log(`  Has timing info: ${!!getResponseEventParams.response.timing}`);
    testRunner.log(`  Has extra info: ${getResponseExtra}`);
    testRunner.log('OPTIONS response:');
    testRunner.log(`  Has timing info: ${!!optionsResponseEventParams.response.timing}`);
    testRunner.log(`  Has extra info: ${optionsResponseExtra}`);

    const message = await dp.Network.getResponseBody({requestId: getRequestEventParams.requestId});
    testRunner.log('Response Body: ' + message.result.body);
    testRunner.completeTest();
  }


  session.evaluate(`
    xhr = new XMLHttpRequest();
    xhr.open('GET', '${url}', true);
    xhr.setRequestHeader('Authorization', '');
    xhr.responseType = 'blob';
    xhr.send();
  `);
  testRunner.log('Evaled fetch command in page');

})
