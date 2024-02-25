(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests request body blobs support.`);

  async function reportRequest(label) {
    var { params : { request, requestId } } = await dp.Network.onceRequestWillBeSent();
    testRunner.log(`[${label}] Data included: ${request.postData !== undefined}, has post data: ${request.hasPostData}`);
    var { result } = await dp.Network.getRequestPostData({ requestId });
    testRunner.log(`[${label}] ${result.postData}`);
  }

  async function SendBlobRequest() {
    var promise = reportRequest('blob');
    await session.evaluateAsync(`
      (function() {
        return fetch('/', { method: "POST", body: new Blob(['string1', 'string2']) });
      })();
    `);
    await promise;
  }

  async function SendClonedRequests() {
    var promise = Promise.all([reportRequest('orginal'), reportRequest('clone')]);
    await session.evaluateAsync(`
      (function() {
        var r1 = new Request('/', { method: 'POST', body: '< Cloned request body >' });
        var r2 = r1.clone();
        return Promise.all(fetch(r1), fetch(r2));
      })();
    `);
    await promise;
  }

  await dp.Network.enable({ maxPostDataSize: 512 });
  await SendBlobRequest();
  await SendClonedRequests();

  testRunner.completeTest();
})
