// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the Main Origin is assigned even if there is no matching Request.\n`);
  await TestRunner.loadModule('security_test_runner');
  await TestRunner.showPanel('security');

  TestRunner.mainTarget.model(Security.SecurityModel)
      .dispatchEventToListeners(
          Security.SecurityModel.Events.SecurityStateChanged,
          new Security.PageSecurityState(
              Protocol.Security.SecurityState.Secure, [], null));

  const page_url = TestRunner.resourceTreeModel.mainFrame.url;
  const page_origin = Common.ParsedURL.extractOrigin(page_url);
  TestRunner.addResult('Page origin: ' + page_origin);
  // Fire a Main Frame Navigation event without firing a NetworkRequest first.
  TestRunner.mainTarget.model(SDK.ResourceTreeModel)
      .dispatchEventToListeners(
          SDK.ResourceTreeModel.Events.MainFrameNavigated, TestRunner.resourceTreeModel.mainFrame);
  // Validate that this set the MainOrigin in the sidebar
  const detectedMainOrigin = Security.SecurityPanel._instance()._sidebarTree._mainOrigin;
  TestRunner.addResult('Detected main origin: ' + detectedMainOrigin);

  // Send subdownload resource requests to other origins.
  const request1 = new SDK.NetworkRequest(0, 'https://foo.test/favicon.ico', page_url, 0, 0, null);
  SecurityTestRunner.dispatchRequestFinished(request1);
  const request2 = new SDK.NetworkRequest(0, 'https://bar.test/bar.css', page_url, 0, 0, null);
  SecurityTestRunner.dispatchRequestFinished(request2);

  // Send one request to the Same Origin as the original page to ensure it appears in the group.
  const request3 = new SDK.NetworkRequest(0, detectedMainOrigin + '/favicon.ico', page_url, 0, 0, null);
  SecurityTestRunner.dispatchRequestFinished(request3);
  SecurityTestRunner.dumpSecurityPanelSidebarOrigins();
  TestRunner.completeTest();
})();
