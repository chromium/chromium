// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function() {
  TestRunner.addResult('Tests the signed exchange information are available when the navigation failed.\n');
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('network');
  SDK.NetworkLog.instance().reset();
  await TestRunner.addIframe('/loading/sxg/resources/sxg-invalid-validity-url.sxg');
  await ConsoleTestRunner.dumpConsoleMessages();
  NetowrkTestRunner.dumpNetworkRequestsWithSignedExchangeInfo();
  TestRunner.completeTest();
})();
