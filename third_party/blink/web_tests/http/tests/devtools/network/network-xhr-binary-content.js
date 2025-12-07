// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

(async function() {
  TestRunner.addResult(`Tests that binary XHR response is not corrupted.\n`);
  await TestRunner.showPanel('network');

  NetworkTestRunner.recordNetwork();
  NetworkTestRunner.makeSimpleXHR('GET', 'resources/binary.data', true, step2);

  async function step2() {
    var request = NetworkTestRunner.networkRequests().pop();
    TestRunner.addResult('request.type: ' + request.resourceType());
    TestRunner.addResult('request.mimeType: ' + request.mimeType);

    var contentData = await request.requestContentData();

    TestRunner.addResult('request.isTextContent: ' + contentData.isTextContent);
    TestRunner.addResult('request.base64: ' + contentData.base64);
    var raw = window.atob(contentData.base64);
    var bytes = [];
    for (var i = 0; i < raw.length; ++i)
      bytes.push(raw.charCodeAt(i));
    TestRunner.addResult('request.content decoded: ' + bytes.join(', '));
    TestRunner.completeTest();
  }
})();
