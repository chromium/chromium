// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SecurityTestRunner} from 'security_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Security from 'devtools/panels/security/security.js';

(async function() {
  TestRunner.addResult(`Tests active mixed content blocking in the security panel.\n`);
  await TestRunner.showPanel('security');

  TestRunner.mainTarget.model(Security.SecurityModel.SecurityModel)
      .dispatchEventToListeners(
        Security.SecurityModel.Events.VisibleSecurityStateChanged,
        new Security.SecurityModel.PageVisibleSecurityState(
          Protocol.Security.SecurityState.Neutral, /* certificateSecurityState= */ null,
          /* safetyTipInfo= */ null, /* securityStateIssueIds= */ ['scheme-is-not-cryptographic']));

  var request = SDK.NetworkRequest.NetworkRequest.create(
      0, 'http://foo.test', 'https://foo.test', 0, 0, null);
  request.setBlockedReason(Protocol.Network.BlockedReason.MixedContent);
  request.mixedContentType = 'blockable';
  SecurityTestRunner.dispatchRequestFinished(request);

  var explanations =
      Security.SecurityPanel.SecurityPanel.instance().mainView.contentElement.getElementsByClassName('security-explanation');
  for (var i = 0; i < explanations.length; i++)
    TestRunner.dumpDeepInnerHTML(explanations[i]);

  TestRunner.completeTest();
})();
