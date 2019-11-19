// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests active mixed content blocking in the security panel.\n`);
  await TestRunner.loadModule('security_test_runner');
  await TestRunner.showPanel('security');

  TestRunner.mainTarget.model(Security.SecurityModel)
      .dispatchEventToListeners(
          Security.SecurityModel.Events.SecurityStateChanged,
          new Security.PageSecurityState(
              Protocol.Security.SecurityState.Secure, [], null));

  var request = new SDK.NetworkRequest(0, 'http://foo.test', 'https://foo.test', 0, 0, null);
  request.setBlockedReason(Protocol.Network.BlockedReason.MixedContent);
  request.mixedContentType = 'blockable';
  SecurityTestRunner.dispatchRequestFinished(request);

  var explanations =
      Security.SecurityPanel._instance()._mainView.contentElement.getElementsByClassName('security-explanation');
  for (var i = 0; i < explanations.length; i++)
    TestRunner.dumpDeepInnerHTML(explanations[i]);

  // Test that the explanations are cleared on navigation. Regression test for https://crbug.com/601944.
  TestRunner.mainTarget.model(SDK.ResourceTreeModel)
      .dispatchEventToListeners(
          SDK.ResourceTreeModel.Events.MainFrameNavigated, TestRunner.resourceTreeModel.mainFrame);
  explanations =
      Security.SecurityPanel._instance()._mainView.contentElement.getElementsByClassName('security-explanation');
  for (var i = 0; i < explanations.length; i++)
    TestRunner.dumpDeepInnerHTML(explanations[i]);

  TestRunner.completeTest();
})();
