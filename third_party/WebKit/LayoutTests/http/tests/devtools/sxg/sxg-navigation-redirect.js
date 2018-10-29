// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function() {
  TestRunner.addResult('Tests the signed exchange information are available when the navigation succeeded after redirect.\n');
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('network');
  SDK.networkLog.reset();
  const url =
      'http://localhost:8000/resources/redirect.php?url=' +
      encodeURIComponent(
        'http://127.0.0.1:8000/loading/sxg/resources/sxg-location.sxg');
  await TestRunner.addIframe(url);
  ConsoleTestRunner.dumpConsoleMessages();
  NetworkTestRunner.dumpNetworkRequestsWithSignedExchangeInfo();
  TestRunner.completeTest();
})();
