// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests conversion of Inspector's resource representation into HAR format.\n`);

  await TestRunner.NetworkAgent.setCacheDisabled(true);
  await TestRunner.reloadPagePromise();
  await TestRunner.evaluateInPagePromise(`
      var xhr = new XMLHttpRequest();
      xhr.open("POST", "${TestRunner.url('resources/post-target.cgi')}", false);
      xhr.setRequestHeader("Content-Type", "text/xml");
      xhr.send("<xml></xml>");
  `);

  function findRequestByURL(url) {
    return NetworkTestRunner.networkRequests().find(request => url.test(request.url()));
  }

  function addCookieHeadersToRequest(request) {
    const c1 = new SDK.Cookie.Cookie('a', 'b');
    c1.addAttribute('path', '/path');
    c1.addAttribute('domain', 'example.com');
    request.addExtraRequestInfo({
      includedRequestCookies: [
        {cookie: c1},
        {cookie: new SDK.Cookie.Cookie('a1', 'b1')},
        {cookie: new SDK.Cookie.Cookie('c1', 'd1')},
      ],
      blockedRequestCookies: [],
      requestHeaders: [{name: 'version', value: 'HTTP/1.1'}],
      connectTiming: {}
    });

    request.responseHeaders = [{
      name: 'Set-Cookie',
      value:
          'x=y; Path=/path; Domain=example.com; Discard; httpOnly; Secure; Version=1\nx1=y1; SameSite=Strict\nz2=y2; SameSite=Lax'
    }];
  }

  function addServiceWorkerInfoToRequest(request) {
    request.fetchedViaServiceWorker = true;
    request.setResponseCacheStorageCacheName('v1');
    request.setServiceWorkerResponseSource('cache-storage');
  }

  const test_url = findRequestByURL(/inspected-page\.html$/);
  addCookieHeadersToRequest(test_url);
  addServiceWorkerInfoToRequest(test_url);
  const requests = NetworkTestRunner.networkRequests();
  var log = await NetworkTestRunner.buildHARLog(requests, {sanitize: false});
  // Filter out favicon.ico requests that only appear on certain platforms.
  log.entries = log.entries.filter(function(entry) {
    return !/favicon\.ico$/.test(entry.request.url);
  });
  log.entries.sort(ApplicationTestRunner.requestURLComparer);
  TestRunner.addObject(log, NetworkTestRunner.HARPropertyFormattersWithSize);
  var pageTimings = log.pages[0].pageTimings;
  TestRunner.completeTest();
})();
