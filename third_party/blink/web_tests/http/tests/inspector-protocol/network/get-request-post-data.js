(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests fetching POST request body.`);

  function SendRequest(method, body) {
    return session.evaluateAsync(`
        new Promise(resolve => {
          const req = new XMLHttpRequest();
          req.open('${method}', '/', true);
          req.setRequestHeader('content-type', 'application/octet-stream');
          req.onreadystatechange = () => resolve();
          req.send(${body});
        });`);
  }

  async function ReportRequest(requestId) {
    const {result, error} = await dp.Network.getRequestPostData({requestId});
    if (error) {
      testRunner.log(`Did not fetch data: ${error.message}`);
    } else {
      testRunner.log(`Data length: ${result.postData.length}`);
    }
  }

  async function SendAndReportRequest(method, body = '') {
    SendRequest(method, body);
    const notification = (await dp.Network.onceRequestWillBeSent()).params;
    const request = notification.request;
    testRunner.log(`Data included: ${request.postData !== undefined}, has post data: ${request.hasPostData}`);
    await dp.Network.onceLoadingFinished();
    await ReportRequest(notification.requestId);
    return notification.requestId;
  }

  await dp.Network.enable({ maxPostDataSize: 512, maxTotalBufferSize: 1025 });
  await SendAndReportRequest('POST', 'new Uint8Array(1024)');
  await SendAndReportRequest('POST', 'new Uint8Array(128)');
  await SendAndReportRequest('POST', '');
  await SendAndReportRequest('GET', 'new Uint8Array(1024)');
  const result = await dp.Network.getRequestPostData({ requestId: 'fake-id' });
  testRunner.log(`Error is: ${result.error.message}`);

  let most_recent_request;
  for (let i = 0; i < 10; i++)
    most_recent_request = await SendAndReportRequest('POST', 'new Uint8Array(64)');

  testRunner.log('Testing request body eviction');
  await SendAndReportRequest('POST', 'new Uint8Array(512)');
  await ReportRequest(most_recent_request);
  testRunner.completeTest();
})
