// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Host from 'devtools/core/host/host.js';

(async function() {
  TestRunner.addResult(
      `Test loading resource for frontend.\n`);
  await TestRunner.showPanel('network');

  var loggedHeaders = {'cache-control': true, 'expires': true, 'last-modified': true, 'pragma': true};

  function testLoadForURL(url, headers, next) {
    TestRunner.addResult('Loading resource from ' + url);
    Host.ResourceLoader.load(url, headers, callback);

    function callback(statusCode, headers, content) {
      TestRunner.addResult('Success: ' + statusCode);
      var headersArray = [];
      for (var name in headers) {
        var nameLower = name.toLowerCase();
        if (nameLower in loggedHeaders)
          headersArray.push(nameLower);
      }
      headersArray.sort();
      TestRunner.addResult('Logged headers: ' + headersArray.join(', '));
      TestRunner.addResult('Content: "' + content + '"');

      TestRunner.deprecatedRunAfterPendingDispatches(next);
    }
  }

  TestRunner.runTestSuite([
    function testNetworkResourceNonCached(next) {
      testLoadForURL('http://localhost:8080/devtools/network/resources/resource.php', undefined, next);
    },

    function testNetworkResourceCached(next) {
      testLoadForURL('http://localhost:8080/devtools/network/resources/resource.php?cached=1', undefined, next);
    },

    function testNetworkResourceIfModifiedSince(next) {
      testLoadForURL(
          'http://localhost:8080/devtools/network/resources/resource.php?cached=1',
          {'if-modified-since': new Date().toUTCString()}, next);
    },

    function testResourceFromIncorrectURL(next) {
      testLoadForURL('resource.php', undefined, next);
    },

    function testResourceWithCookie(next) {
      NetworkTestRunner.makeSimpleXHR(
          'GET', 'http://127.0.0.1:8000/devtools/network/resources/set-cookie.php', false, callback);

      function callback() {
        testLoadForURL('http://127.0.0.1:8000/devtools/network/resources/print-cookie.php', undefined, next);
      }
    }
  ]);
})();
