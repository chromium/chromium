// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      'Verifies that Network.*ExtraInfo events get assigned to the correct SDK.NetworkRequest instance in the case of cross origin redirects.');
  await TestRunner.loadModule('network_test_runner');

  await TestRunner.evaluateInPageAsync(`
new Promise(resolve => {
  xhr = new XMLHttpRequest();
  xhr.open('GET', 'http://redirect-one.test:8000/devtools/network/resources/redirect-1.php');
  xhr.send();
  xhr.onreadystatechange = () => {
    if (xhr.readyState === XMLHttpRequest.DONE)
      resolve();
  };
});
`);

  const reqs = SDK.networkLog.requests().map(request => {
    return {
      url: request.url(),
      hasExtraRequestInfo: request.hasExtraRequestInfo(),
      hasExtraResponseInfo: request.hasExtraResponseInfo(),
      requestHostHeader: request.requestHeaderValue('host'),
      responseXDevToolsRedirectHeader:
          request.responseHeaderValue('x-devtools-redirect')
    };
  });
  TestRunner.addResult(JSON.stringify(reqs, null, 2));
  TestRunner.completeTest();
})();
