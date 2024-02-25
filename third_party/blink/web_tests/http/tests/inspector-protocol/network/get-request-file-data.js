(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
    <form action="/" enctype="multipart/form-data" method="post">
      <input type="file" name="file" />
    </form>`,
    `Tests request body blobs support.`);
  dp.Network.enable();

  session.evaluate(`
    const file = new File(['Hello World!'], 'file.txt', {
      type: 'text/plain',
      lastModified: new Date(),
    });

    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(file);
    document.querySelector('input[type="file"]').files = dataTransfer.files;
    document.querySelector('form').submit();
  `);

  const {params: {request, requestId}} =
      await dp.Network.onceRequestWillBeSent();
  testRunner.log(`Data included: ${
      request.postData !== undefined}, has post data: ${request.hasPostData}`);
  const {result} = await dp.Network.getRequestPostData({requestId});
  testRunner.log(
      result.postData.replace(/WebKitFormBoundary[A-Za-z0-9]*/g, 'boundary'));

  testRunner.completeTest();
})
