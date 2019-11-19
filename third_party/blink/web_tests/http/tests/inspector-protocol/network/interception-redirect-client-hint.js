// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  document.head.innerHTML = ' <meta http-equiv="Accept-CH" content="DPR"> <meta http-equiv="Accept-CH-Lifetime" content="1">';
  var {page, session, dp} = await testRunner.startBlank(`Test that UA client hints are added on redirect.\n`);

  dp.Network.enable();
  dp.Page.enable();

  dp.Network.setRequestInterception({patterns: [{}]});
  dp.Runtime.evaluate({expression: `
    document.body.innerHTML = '<iframe src="http://127.0.1.1:8000/whatever"></iframe>';
  `});

  dp.Network.onRequestIntercepted(event => {
    const request = event.params.request;
    testRunner.log(`Got request: ${request.method} ${request.url}`);
    for (const header of ["sec-ch-ua", "dpr"]) {
      if (`${request.headers[header]}` != "undefined") {
        testRunner.log(`${header}: ${request.headers[header]}`);
      }
    }
    const url = `${request.url}`;
    if (url.includes("empty.html")) {
      testRunner.completeTest()
    }
  });
  let params = (await dp.Network.onceRequestIntercepted()).params;
  const response = "HTTP/1.1 303 See other\r\n" +
      "Location: http://127.0.0.1:8000/devtools/resources/empty.html\r\n\r\n";
  dp.Network.continueInterceptedRequest({interceptionId: params.interceptionId, rawResponse: btoa(response)});
  params = (await dp.Network.onceRequestIntercepted()).params;
  dp.Network.continueInterceptedRequest({interceptionId: params.interceptionId});
});

