// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that the content of resources with JSON MIME types can be accessed.`);
  TestRunner.addResult('When loaded by asynchronous XHR requests (Bug 80684) or within iframes/documents.\n');

  await TestRunner.showPanel('network');

  NetworkTestRunner.recordNetwork();

  function lastRequest() {
    return NetworkTestRunner.networkRequests().pop();
  }

  function reportRequest(request) {
    TestRunner.addResult('request.url: ' + request.url());
    TestRunner.addResult('request.type: ' + request.resourceType());
    TestRunner.addResult('request.mimeType: ' + request.mimeType);
  }

  function testType(contentType, callback) {
    var encodedContentType = encodeURIComponent(contentType);

    function step2() {
      var request = lastRequest();
      reportRequest(request);
      request.requestContent().then(step3);
    }

    function step3({ content, error, isEncoded }) {
      TestRunner.addResult('request.content: ' + content);
      TestRunner.addIframe('resources/json.php?type=' + encodedContentType).then(step4);
    }

    function step4() {
      var request = lastRequest();
      reportRequest(request);
      request.requestContent().then(step5);
    }

    function step5({ content, error, isEncoded }) {
      TestRunner.addResult('request.content: ' + content);
      callback();
    }

    NetworkTestRunner.makeSimpleXHR('GET', 'resources/json.php?type=' + encodedContentType, true, step2);
  }

  TestRunner.runTestSuite([
    function test1(next) {
      testType('application/json', next);
    },
    function test2(next) {
      testType('application/vnd.document+json', next);
    }
  ]);
})();
