// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that the panel shows the correct text for non-cryptographic secure origins\n`);
  await TestRunner.loadModule('security_test_runner');
  await TestRunner.showPanel('security');

  var request1 = new SDK.NetworkRequest(0, 'chrome-test://test', 'chrome-test://test', 0, 0, null);
  request1.setSecurityState(Protocol.Security.SecurityState.Secure);
  SecurityTestRunner.dispatchRequestFinished(request1);

  Security.SecurityPanel._instance()._sidebarTree._elementsByOrigin.get('chrome-test://test').select();

  TestRunner.addResult('Panel on origin view:');
  TestRunner.dumpDeepInnerHTML(Security.SecurityPanel._instance()._visibleView.contentElement);

  TestRunner.completeTest();
})();
