// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests query string extraction.\n`);
  await TestRunner.showPanel('network');

  function checkURL(url) {
    var request = SDK.NetworkRequest.NetworkRequest.create(url, url, '', '', '');
    TestRunner.addResult('URL: ' + url);
    TestRunner.addResult('Query: ' + request.queryString());
    TestRunner.addResult('');
  }

  checkURL('http://webkit.org');
  checkURL('http://webkit.org?foo');
  checkURL('http://webkit.org#bar');
  checkURL('http://webkit.org?foo#bar');
  checkURL('http://webkit.org?foo=bar?baz');
  checkURL('http://webkit.org?foo?bar#baz');

  TestRunner.completeTest();
})();
