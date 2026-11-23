(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startURL(
      '../resources/test-page.html',
      `Verify that only one request is made for basic script requests with integrity attribute.\n`);

  await dp.Network.enable();
  await dp.Runtime.enable();

  const scriptRequests = [];
  dp.Network.onRequestWillBeSent(event => {
    if (/call-success\.js/.test(event.params.request.url)) {
      scriptRequests.push(event.params.request.url);
    }
  });

  await dp.Runtime.evaluate({
    expression: `
      // Regression test for https://crbug.com/573269.
      function loadIFrame() {
        var iframe = document.createElement('iframe');
        iframe.src = '${
        testRunner.url('resources/call-success-with-integrity-frame.html')}';
        document.body.appendChild(iframe);
      }
    `,
    objectGroup: 'console',
  });

  const consoleMessage = dp.Runtime.onceConsoleAPICalled();
  await dp.Runtime.evaluate(
      {expression: 'loadIFrame()', objectGroup: 'console'});
  const {args, stackTrace} = (await consoleMessage).params;

  if (scriptRequests.length !== 1) {
    testRunner.log(`FAIL: expected 1 request for call-success.js, got ${
        scriptRequests.length}: ${scriptRequests.join(', ')}`);
  }
  const {url, lineNumber} = stackTrace.callFrames[0];
  testRunner.log(`${url.substring(url.lastIndexOf('/') + 1)}:${
      lineNumber + 1} ${args.map(arg => arg.value).join(' ')}`);
  testRunner.completeTest();
})
