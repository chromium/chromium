(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
    `Tests binary headers in Fetch.fulfillRequest.`);

  const FetchHelper = await testRunner.loadScript('resources/fetch-test.js');

  const helper = new FetchHelper(testRunner, testRunner.browserP());

  async function buildBinaryHeaders(headers) {
    const utf8Encoder = new TextEncoder();
    const blob = new Blob([utf8Encoder.encode(headers.join('\0')).buffer]);
    const fileReader = new FileReader();
    await new Promise(fulfill => {
      fileReader.onload = fulfill;
      fileReader.readAsDataURL(blob);
    });
    const dataURL = fileReader.result;
    // Strip the "data:*/*;base64," prefix from dataURL.
    return dataURL.substr(dataURL.indexOf(',') + 1);
  }

  await helper.enable();

  // Assure UTF-8 in Location header will get to the redirect and then
  // get URL-escaped.
  helper.onceRequest().fulfill({
      responseCode: 303,
      binaryResponseHeaders: await buildBinaryHeaders(['Location: /step2?тест']),
  });

  helper.onceRequest().fulfill({
      responseCode: 200,
      responseHeaders: [{name: 'content-type', value: 'text/html'}],
      body: btoa('<html></html>')
  });

  await session.navigate("http://a.test/step1");

  testRunner.log(`Navigated to: ${await session.evaluate('location.href')}`);

  // Assure Latin1 arrives to the page intact.
  helper.onceRequest().fulfill({
    responseCode: 200,
    binaryResponseHeaders: btoa('X-Test-Header: hétérogénéité\0X-Another-Header: value'),
    body: ""
  });

  const headerValues = await session.evaluateAsync(`
      fetch('/subresource').then(r =>
          [r.headers.get('X-Test-Header'), r.headers.get('X-Another-Header')])
  `);

  testRunner.log(`Subresource headers: ${headerValues.join(', ')}`);
  testRunner.completeTest();
})
