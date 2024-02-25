// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Network from 'devtools/panels/network/network.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
    'use strict';
    TestRunner.addResult(`Tests that serviceworker timings are displayed correctly.\n`);
    await TestRunner.showPanel('network');

    await TestRunner.reloadPagePromise();
    // Reload page

    function setRequestValues(request) {
      request.setRequestHeaders([{name: 'Request', value: 'request-value'}]);
      request.setRequestHeadersText('GET http://example.com/inspector-test.js HTTP/1.1\r\nRequest: headers-text');

      request.responseHeaders = [{name: 'Response', value: 'response-value'}];
      request.responseHeadersText = 'HTTP/1.1 200 OK\r\nResponse: headers-text';

      request.requestMethod = 'GET';
      request.mimeType = 'text/html';
      request.statusCode = 200;
      request.statusText = 'OK';
      request.setPriority('VeryHigh');

      // sample timing values used here are copied from a real request
      request.setIssueTime(559396.428899);
      request.endTime = 559396.439805;
      request.fetchedViaServiceWorker = true;
      request.timing = {
        'connectEnd': -1,
        'connectStart': -1,
        'dnsEnd': -1,
        'dnsStart': -1,
        'proxyEnd': -1,
        'proxyStart': -1,
        'pushEnd': 0,
        'pushStart': 0,
        'receiveHeadersEnd': 6.718,
        'requestTime': 559396.428899,
        'sendEnd': 0.003,
        'sendStart': 0.003,
        'sslEnd': -1,
        'sslStart': -1,
        'workerFetchStart': 0.484,
        'workerReady': 0.484,
        'workerRespondWithSettled': 5.959,
        'workerStart': 0.003,
      };
    }

    const fakeInitiator = {
      type: 'parser',
      url: 'http://example.com/inspector-test.js',
      lineNumber: 117
    };
    var testRequest = SDK.NetworkRequest.NetworkRequest.create(
        'testRequest', 'http://example.com/inspector-test.js',
        'http://example.com/fake-document-url', 1, 1, fakeInitiator);
    setRequestValues(testRequest);

    const calculator = Network.NetworkPanel.NetworkPanel.instance().calculator;
    const tableElement = Network.RequestTimingView.RequestTimingView.createTimingTable(testRequest, calculator);

    for (const element of tableElement.getElementsByTagName('td')) {
      const content = element.textContent;
      // Exclude timings that depend on runtime calculations
      if (!content.includes('Queued at') && !content.includes('Started at')) {
        TestRunner.addResult(element.textContent);
      }
    }

    TestRunner.completeTest();
  })();
