// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests filtering of requests suitable for HAR.\n`);
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('network');

  function checkURL(url) {
    var request = SDK.NetworkRequest.create(url, url, '', '', '');
    var result = Network.NetworkLogView.HTTPRequestsFilter(request);
    TestRunner.addResult((result ? '' : 'Non-') + 'HTTP request URL: ' + url);
  }

  checkURL('http://webkit.org');
  checkURL('https://webkit.org');
  checkURL('ws://webkit.org');
  checkURL('wss://webkit.org');

  TestRunner.addResult('');

  checkURL('data:text/plain,foo');
  checkURL('file:///etc/passwd');
  checkURL('javascript:void()');
  checkURL('invalid');

  TestRunner.completeTest();
})();
