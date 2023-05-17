// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

(async function() {
  await TestRunner.addResult(`Tests that XHR redirects preserve request body.`);
  await TestRunner.showPanel('network');

  var offset;

  NetworkTestRunner.recordNetwork();
  offset = NetworkTestRunner.networkRequests().length;
  NetworkTestRunner.makeSimpleXHRWithPayload('POST', 'resources/redirect.cgi?status=301&ttl=1', true, 'LOST', step2);

  function step2() {
    NetworkTestRunner.networkRequests()[offset].requestContent().then(step3);
  }

  function step3() {
    NetworkTestRunner.makeSimpleXHRWithPayload(
        'POST', 'resources/redirect.cgi?status=307&ttl=1', true, 'PRESERVED', step4);
  }


  function step4() {
    NetworkTestRunner.networkRequests()[offset + 2].requestContent().then(
        step5);
  }
  async function step5() {
    var requests = NetworkTestRunner.networkRequests();
    for (var i = 0; i < requests.length; ++i) {
      var request = requests[i];
      var requestMethod = request.requestMethod;
      var actualMethod = request.responseHeaderValue('request-method');
      var formData = await request.requestFormData();
      var body = `[${formData || ''}]`;
      TestRunner.addResult(requestMethod + ' ' + request.url());
      TestRunner.addResult('  actual http method was: ' + actualMethod);
      TestRunner.addResult('  request body: ' + body);
      TestRunner.addResult('');
    }
    TestRunner.completeTest();
  }
})();
