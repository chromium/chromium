// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

(async function() {
  TestRunner.addResult(`Tests that XHR redirects preserve http-method.\n`);
  await TestRunner.showPanel('network');

  var offset;

  NetworkTestRunner.recordNetwork();
  offset = NetworkTestRunner.networkRequests().length;
  NetworkTestRunner.makeSimpleXHR('PUT', 'resources/redirect.cgi?status=301&ttl=3', true, step2);

  function step2() {
    NetworkTestRunner.networkRequests()[offset].requestContent().then(step3);
  }

  function step3() {
    var requests = NetworkTestRunner.networkRequests();
    for (var i = 0; i < requests.length; ++i) {
      var request = requests[i];
      var requestMethod = request.requestMethod;
      var actualMethod = request.responseHeaderValue('request-method');
      TestRunner.addResult(requestMethod + ' ' + request.url());
      TestRunner.addResult('  actual http method was: ' + actualMethod);
      TestRunner.addResult('');
    }
    TestRunner.completeTest();
  }
})();
