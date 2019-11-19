// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadModule('security_test_runner');
  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.showPanel('security');

  const pageSecurityState = new Security.PageSecurityState(Protocol.Security.SecurityState.Secure, /* explanations= */ [], null);

  TestRunner.mainTarget.model(Security.SecurityModel).dispatchEventToListeners(
    Security.SecurityModel.Events.SecurityStateChanged, pageSecurityState);
  const request = new SDK.NetworkRequest(0, 'http://foo.test', 'https://foo.test', 0, 0, null);
  request.setBlockedReason(Protocol.Network.BlockedReason.MixedContent);
  request.mixedContentType = 'blockable';
  SecurityTestRunner.dispatchRequestFinished(request);
  const securityPanel = runtime.sharedInstance(Security.SecurityPanel);
  await AxeCoreTestRunner.runValidation(securityPanel._mainView.contentElement);

  TestRunner.completeTest();
})();
