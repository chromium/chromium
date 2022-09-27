// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function() {
  TestRunner.addResult('Tests the signed exchange information are available when the navigation succeeded after redirect.\n');
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('network');
  NetworkTestRunner.networkLog().reset();
  const url =
      'http://localhost:8000/resources/redirect.php?url=' +
      encodeURIComponent(
        'http://127.0.0.1:8000/loading/sxg/resources/sxg-location.sxg');
  await TestRunner.addIframe(url);
  await ConsoleTestRunner.dumpConsoleMessages();
  NetworkTestRunner.dumpNetworkRequestsWithSignedExchangeInfo();
  TestRunner.completeTest();
})();
