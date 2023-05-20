// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

(async function() {
  TestRunner.addResult(`Tests XHR network resource payload is not corrupted by transcoding.\n`);
  await TestRunner.showPanel('network');

  var payload = '\u201AFoo\u201B';

  NetworkTestRunner.recordNetwork();
  NetworkTestRunner.makeSimpleXHRWithPayload('POST', 'resources/resource.php?foo', true, payload, step2);

  async function step2() {
    var request = NetworkTestRunner.networkRequests().pop();
    TestRunner.addResult(request.url());
    TestRunner.assertEquals('foo', request.queryString(), 'Unexpected resource query.');
    TestRunner.assertEquals(payload, await request.requestFormData(), 'Payload corrupted.');
    TestRunner.completeTest();
  }
})();
