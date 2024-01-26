(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test to make sure if an xhr is fetched with the response as a blob and cross origin devtools can get body.`);

  const protocolMessages = [];
  const originalDispatchMessage = DevToolsAPI.dispatchMessage;
  DevToolsAPI.dispatchMessage = (message) => {
    protocolMessages.push(message);
    originalDispatchMessage(message);
  };
  window.onerror = (msg) => testRunner.log('onerror: ' + msg);
  window.onunhandledrejection = (e) => testRunner.log('onunhandledrejection: ' + e.reason);
  let errorForLog = new Error();
  setTimeout(() => {
    testRunner.log(protocolMessages);
    testRunner.die('Timeout', errorForLog);
  }, 28000);

  // This url should be cross origin.
  const url = 'https://127.0.0.1:8443/inspector-protocol/resources/cors-data.php';

  await dp.Network.enable();
  testRunner.log('Network Enabled');

  const events = [];

  const gotAllEvents = new Promise((resolve) => {
    let eventHandler = (event) => {
      events.push(event);
      if (events.length == 10) {
        resolve();
      }
    };
    dp.Network.onRequestWillBeSent(eventHandler);
    dp.Network.onRequestWillBeSentExtraInfo(eventHandler);
    dp.Network.onResponseReceived(eventHandler);
    dp.Network.onResponseReceivedExtraInfo(eventHandler);
    dp.Network.onLoadingFinished(eventHandler);
  });

  session.evaluate(`
    xhr = new XMLHttpRequest();
    xhr.open('GET', '${url}', true);
    xhr.setRequestHeader('Authorization', '');
    xhr.responseType = 'blob';
    xhr.send();
  `);
  testRunner.log('Evaled fetch command in page');
  await gotAllEvents;
  errorForLog = new Error();

  let getRequestEventParams;
  let optionsRequestEventParams;
  let getRequestExtra = false;
  let optionsRequestExtra = false;
  let getResponseExtra = false;
  let optionsResponseExtra = false;
  let getResponseEventParams;
  let optionsResponseEventParams;

  events.forEach((event) => {
    if (event.method == 'Network.requestWillBeSent') {
      if (event.params.request.method === 'GET') {
        getRequestEventParams = event.params;
      } else if (event.params.request.method === 'OPTIONS') {
        optionsRequestEventParams = event.params;
        optionsRequestInitiatorRequestId = event.params.initiator.requestId;
      }
    }
  });
  events.forEach((event) => {
    if (event.method == 'Network.requestWillBeSentExtraInfo') {
      if (event.params.requestId === getRequestEventParams.requestId) {
        getRequestExtra = true;
      } else if (
          event.params.requestId === optionsRequestEventParams.requestId) {
        optionsRequestExtra = true;
      }
    }
  });
  events.forEach((event) => {
    if (event.method == 'Network.responseReceivedExtraInfo') {
      if (event.params.requestId === getRequestEventParams.requestId) {
        getResponseExtra = true;
      } else if (
          event.params.requestId === optionsRequestEventParams.requestId) {
        optionsResponseExtra = true;
      }
    }
  });
  events.forEach((event) => {
    if (event.method == 'Network.responseReceived') {
      if (event.params.requestId === getRequestEventParams.requestId) {
        getResponseEventParams = event.params;
      } else if (
          event.params.requestId === optionsRequestEventParams.requestId) {
        optionsResponseEventParams = event.params;
      }
    }
  });

  if (getResponseEventParams && optionsResponseEventParams) {
    printResultsAndFinish();
  }

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
    errorForLog = new Error();
    if (message.error) testRunner.log(message.error);
    testRunner.log('Response Body: ' + message.result.body);
    testRunner.completeTest();
  }
})
