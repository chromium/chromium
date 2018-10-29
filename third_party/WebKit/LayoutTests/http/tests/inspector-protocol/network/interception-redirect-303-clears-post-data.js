// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Test to ensure devtools clears post data after getting HTTP 303 through interception response.\n`);

  dp.Network.enable();
  dp.Page.enable();

  dp.Network.setRequestInterception({patterns: [{}]});

  dp.Runtime.evaluate({expression: `
    document.body.innerHTML = '<form id="form" method="post" action="/my-path"><input type="text" name="foo" value="bar" /></form>';
    var form = document.getElementById('form');
    form.submit();
  `});

  dp.Network.onRequestIntercepted(event => {
    const request = event.params.request;
    testRunner.log(`Got request: ${request.method} ${request.url}`);
    if (request.postData)
      testRunner.log("Post Data: " + request.postData);
    for (const header of ["Origin", "Content-Type"]) {
      testRunner.log(`${header}: ${request.headers[header]}`);
    }
  });
  let params = (await dp.Network.onceRequestIntercepted()).params;
  const response = "HTTP/1.1 303 See other\r\n" +
      "Location: http://127.0.0.1:8000/devtools/resources/empty.html\r\n\r\n";
  dp.Network.continueInterceptedRequest({interceptionId: params.interceptionId, rawResponse: btoa(response)});
  params = (await dp.Network.onceRequestIntercepted()).params;
  dp.Network.continueInterceptedRequest({interceptionId: params.interceptionId});
  dp.Page.onLoadEventFired(() => testRunner.completeTest());
});