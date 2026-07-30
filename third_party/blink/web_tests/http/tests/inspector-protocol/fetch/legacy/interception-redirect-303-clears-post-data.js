// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Test to ensure devtools clears post data after getting HTTP 303 through interception response.\n`);

  dp.Network.enable();
  dp.Page.enable();

  dp.Fetch.enable({patterns: [{}]});

  dp.Runtime.evaluate({
    expression: `
    document.body.innerHTML = '<form id="form" method="post" action="/my-path"><input type="text" name="foo" value="bar" /></form>';
    var form = document.getElementById('form');
    form.submit();
  `
  });

  dp.Fetch.onRequestPaused(event => {
    const request = event.params.request;
    testRunner.log(`Got request: ${request.method} ${request.url}`);
    if (request.postData)
      testRunner.log('Post Data: ' + request.postData);
    for (const header of ['Origin', 'Content-Type']) {
      testRunner.log(`${header}: ${request.headers[header]}`);
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
  dp.Page.onLoadEventFired(() => testRunner.completeTest());
});
