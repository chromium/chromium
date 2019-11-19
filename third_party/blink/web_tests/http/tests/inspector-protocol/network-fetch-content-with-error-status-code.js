(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Test to make sure if an xhr is fetched with the response as a blob and cross origin devtools can get body.`);

  var requestWillBeSentPromise = dp.Network.onceRequestWillBeSent();
  // This url should be cross origin.
  const url = 'https://127.0.0.1:8443/inspector-protocol/resources/cors-data.php';

  await dp.Network.enable();
  testRunner.log('Network Enabled');

  session.evaluate(`
    xhr = new XMLHttpRequest();
    xhr.open('GET', '${url}', true);
    xhr.setRequestHeader('Authorization', '');
    xhr.responseType = 'blob';
    xhr.send();
  `);
  testRunner.log('Evaled fetch command in page');

  var event = await requestWillBeSentPromise;
  testRunner.log('Request will be sent');
  testRunner.log('Request Method (should be GET): ' + event.params.request.method);
  var requestId = event.params.requestId;

  var event = await dp.Network.onceResponseReceived();
  testRunner.log('Got response received');
  testRunner.log('requestId is the same as requestWillBeSent: ' + (requestId === event.params.requestId));

  var message = await dp.Network.getResponseBody({requestId});
  testRunner.log('Response Body: ' + message.result.body);

  testRunner.completeTest();
})
