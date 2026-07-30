// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // This one should not be kept per redirect since http-equiv accept-ch doesn't
  // persist.
  document.head.innerHTML = ' <meta http-equiv="Accept-CH" content="DPR"> ';
  var {page, session, dp} = await testRunner.startBlank(
      `Test that UA client hints are added on redirect.\n`);

  dp.Network.enable();
  dp.Page.enable();

  dp.Fetch.enable({patterns: [{}]});
  dp.Runtime.evaluate({
    expression: `
    document.body.innerHTML = '<iframe src="http://127.0.1.1:8000/whatever"></iframe>';
  `
  });

  dp.Fetch.onRequestPaused(event => {
    const request = event.params.request;
    testRunner.log(`Got request: ${request.method} ${request.url}`);
    for (const header of ['sec-ch-ua', 'dpr']) {
      if (`${request.headers[header]}` != 'undefined') {
        testRunner.log(`${header}: ${request.headers[header]}`);
      }
    }
    const url = `${request.url}`;
    if (url.includes('empty.html')) {
      testRunner.completeTest()
    }
  });
  let params = (await dp.Fetch.onceRequestPaused()).params;
  dp.Fetch.fulfillRequest({
    requestId: params.requestId,
    responseCode: 303,
    responsePhrase: 'See other',
    responseHeaders: [{
      name: 'Location',
      value: 'http://127.0.0.1:8000/devtools/resources/empty.html'
    }]
  });
  params = (await dp.Fetch.onceRequestPaused()).params;
  dp.Fetch.continueRequest({requestId: params.requestId});
});
