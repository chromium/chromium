// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      'Verifies that Network.*ExtraInfo events get assigned to the correct SDK.NetworkRequest.NetworkRequest instance in the case of cross origin redirects.');

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

  const reqs = NetworkTestRunner.networkRequests()
                   .map(request => {
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
