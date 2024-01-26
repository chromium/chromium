(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests MIME type sniffing of fetch response`);

  const FetchHelper = await testRunner.loadScript('resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  await helper.enable();

  const image_base64 =
      '/9j/4AAQSkZJRgABAQAASABIAAD/2wBDAAMCAgICAgMCAgIDAwMDBAYEBAQEBAgGBgUGCQgKCgkI' +
      'CQkKDA8MCgsOCwkJDRENDg8QEBEQCgwSExIQEw8QEBD/2wBDAQMDAwQDBAgEBAgQCwkLEBAQEBAQ' +
      'EBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBD/wAARCAAQABADASIA' +
      'AhEBAxEB/8QAFQABAQAAAAAAAAAAAAAAAAAAAAj/xAAUEAEAAAAAAAAAAAAAAAAAAAAA/8QAFQEB' +
      'AQAAAAAAAAAAAAAAAAAABAf/xAAUEQEAAAAAAAAAAAAAAAAAAAAA/9oADAMBAAIRAxEAPwC4AAlT' +
      'f//Z';

  const testCases = [
    // Should be auto-detected to image/jpeg.
    [],
    // Should be faked as text/plain.
    [
      {name: 'X-Content-Type-Options', value: 'nosniff'}
    ],
    // Should be kept as is.
    [
      {name: 'Content-Type', value: 'text/plain'},
      {name: 'X-Content-Type-Options', value: 'nosniff'}
    ],
    // Should be auto-detected as image/jpeg.
    [
      {name: 'Content-Type', value: 'text/plain'},
    ],
    // Should be kept as image/png.
    [
      {name: 'Content-Type', value: 'image/png'},
    ]
  ];

  helper.onceRequest('http://test.com/').fulfill({
    responseCode: 200,
    responseHeaders: [{name: 'Content-Type', value: 'text/html'}],
    body: btoa('<html></html>')
  });
  dp.Network.onResponseReceived(event => {
    const response = event.params.response;
    testRunner.log(`${response.url}, reported MIME type: ${response.mimeType}`);
  });
  await session.navigate('http://test.com/');
  await dp.Network.enable();

  let count = 0;
  for (const headers of testCases) {
    const url = `http://test.com/image${++count}.jpg`;
    helper.onceRequest(url).fulfill({
      responseCode: 200,
      responseHeaders: headers,
      body: image_base64
    });
    session.evaluate(`(function() {
      const img = document.createElement('img');
      img.src = '${url}';
      document.body.appendChild(img);
    })()`);
    await dp.Network.onceResponseReceived();
  }
  testRunner.completeTest();
})

