// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that the panel shows the correct text for non-cryptographic secure origins\n`);
  await TestRunner.loadTestModule('security_test_runner');
  await TestRunner.showPanel('security');

  var request1 = SDK.NetworkRequest.create(
      0, 'chrome-test://test', 'chrome-test://test', 0, 0, null);
  request1.setSecurityState(Protocol.Security.SecurityState.Secure);
  SecurityTestRunner.dispatchRequestFinished(request1);

  Security.SecurityPanel.instance().sidebarTree.elementsByOrigin.get('chrome-test://test').select();

  TestRunner.addResult('Panel on origin view:');
  TestRunner.dumpDeepInnerHTML(Security.SecurityPanel.instance().visibleView.contentElement);

  TestRunner.completeTest();
})();
