// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function() {
  TestRunner.addResult('Tests the signed exchange information are available when the navigation succeeded.\n');
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('network');
  SDK.NetworkLog.instance().reset();
  await TestRunner.addIframe('/loading/sxg/resources/sxg-location.sxg');
  await ConsoleTestRunner.dumpConsoleMessages();
  NetworkTestRunner.dumpNetworkRequestsWithSignedExchangeInfo();
  TestRunner.completeTest();
})();
