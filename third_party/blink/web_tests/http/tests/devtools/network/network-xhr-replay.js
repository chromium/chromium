// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

(async function() {
  TestRunner.addResult(`Tests XHR replaying. Bug 95187\n`);
  await TestRunner.showPanel('network');

  function lastRequest() {
    return NetworkTestRunner.networkRequests().pop();
  }

  async function dumpRequest(request) {
    TestRunner.addResult('Dumping request: ');
    TestRunner.addResult('    url: ' + request.url());
    var formData = await request.requestFormData();
    if (formData)
      TestRunner.addResult('    requestFormData: ' + formData);
    TestRunner.addResult('    requestMethod: ' + request.requestMethod);
    TestRunner.addResult('    test request header value: ' + request.requestHeaderValue('headerName'));
  }

  function assertRequestEqual(request1, request2) {
    TestRunner.assertEquals(request1.url(), request2.url(), 'Requests have different url');
    TestRunner.assertEquals(
        request1.requestFormData, request2.requestFormData, 'Requests have different requestFormData');
    TestRunner.assertEquals(request1.requestMethod, request2.requestMethod, 'Requests have different requestMethod');
    TestRunner.assertEquals(
        request1.requestHeadersText, request2.requestHeadersText, 'Requests have different requestHeadersText');
  }

  async function testXHRReplay(method, url, async, user, password, headers, withCredentials, payload, type, callback) {
    NetworkTestRunner.makeXHR(method, url, async, user, password, headers, withCredentials, payload, type);

    var {request: originalRequest} = await TestRunner.waitForEvent(
        NetworkTestRunner.NetworkLogEvents.RequestAdded,
        NetworkTestRunner.networkLog());
    await dumpRequest(originalRequest);
    TestRunner.NetworkAgent.replayXHR(originalRequest.requestId());
    var {request: replayedRequest} = await TestRunner.waitForEvent(
        NetworkTestRunner.NetworkLogEvents.RequestAdded,
        NetworkTestRunner.networkLog());

    assertRequestEqual(originalRequest, replayedRequest);
    callback();
  }


  TestRunner.runTestSuite([
    function testGetStaticAsync(next) {
      testXHRReplay(
          'GET', 'resources/empty.html', true, 'user', 'password', [['headerName', 'headerValueA']], false, undefined,
          undefined, next);
    },

    function testGetStaticSync(next) {
      testXHRReplay(
          'GET', 'resources/empty.html', false, 'user', 'password', [['headerName', 'headerValueB']], false, undefined,
          undefined, next);
    },

    function testGetCachedAsync(next) {
      testXHRReplay(
          'GET', 'resources/random-cached.php', true, 'user', 'password', [['headerName', 'headerValueC']], false,
          undefined, undefined, next);
    },

    function testGetCachedSync(next) {
      testXHRReplay(
          'GET', 'resources/random-cached.php', false, 'user', 'password', [['headerName', 'headerValueD']], false,
          undefined, undefined, next);
    },

    function testGetRandomAsync(next) {
      testXHRReplay(
          'GET', 'resources/random.php', true, 'user', 'password', [['headerName', 'headerValueE']], false, undefined,
          undefined, next);
    },

    function testGetRandomSync(next) {
      testXHRReplay(
          'GET', 'resources/random.php', false, 'user', 'password', [['headerName', 'headerValueF']], false, undefined,
          undefined, next);
    },

    function testPostAsync(next) {
      testXHRReplay(
          'POST', 'resources/random.php', true, 'user', 'password', [['headerName', 'headerValueG']], false, 'payload',
          undefined, next);
    },

    function testPostSync(next) {
      testXHRReplay(
          'POST', 'resources/random.php', false, 'user', 'password', [['headerName', 'headerValueH']], false, 'payload',
          undefined, next);
    }
  ]);
})();
