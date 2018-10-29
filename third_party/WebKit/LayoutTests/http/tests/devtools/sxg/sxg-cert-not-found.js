// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function() {
  TestRunner.addResult('Tests the signed exchange information are available when the certificate file is not available.\n');
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('network');
  SDK.networkLog.reset();
  await TestRunner.addIframe('/loading/sxg/resources/sxg-cert-not-found.sxg');
  ConsoleTestRunner.dumpConsoleMessages();
  NetworkTestRunner.dumpNetworkRequestsWithSignedExchangeInfo();
  TestRunner.completeTest();
})();
