// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests active mixed content blocking in the security panel.\n`);
  await TestRunner.loadTestModule('security_test_runner');
  await TestRunner.showPanel('security');

  TestRunner.mainTarget.model(Security.SecurityModel)
      .dispatchEventToListeners(
        Security.SecurityModel.Events.VisibleSecurityStateChanged,
        new Security.PageVisibleSecurityState(
          Protocol.Security.SecurityState.Secure, /* certificateSecurityState= */ null,
          /* safetyTipInfo */ null, /* securityStateIssueIds= */ ['scheme-is-not-cryptographic']));

  var request = SDK.NetworkRequest.create(
      0, 'http://foo.test', 'https://foo.test', 0, 0, null);
  request.setBlockedReason(Protocol.Network.BlockedReason.MixedContent);
  request.mixedContentType = 'blockable';
  SecurityTestRunner.dispatchRequestFinished(request);

  var explanations =
      Security.SecurityPanel.instance().mainView.contentElement.getElementsByClassName('security-explanation');
  for (var i = 0; i < explanations.length; i++)
    TestRunner.dumpDeepInnerHTML(explanations[i]);

  // Test that the explanations are cleared on navigation. Regression test for https://crbug.com/601944.
  TestRunner.mainTarget.model(SDK.ResourceTreeModel)
      .dispatchEventToListeners(
          SDK.ResourceTreeModel.Events.MainFrameNavigated, TestRunner.resourceTreeModel.mainFrame);
  explanations =
      Security.SecurityPanel.instance().mainView.contentElement.getElementsByClassName('security-explanation');
  for (var i = 0; i < explanations.length; i++)
    TestRunner.dumpDeepInnerHTML(explanations[i]);

  TestRunner.completeTest();
})();
