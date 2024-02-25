// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SecurityTestRunner} from 'security_test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Security from 'devtools/panels/security/security.js';

(async function() {
  await TestRunner.showPanel('security');

  const pageVisibleSecurityState = new Security.SecurityModel.PageVisibleSecurityState(
    Protocol.Security.SecurityState.Secure, /* certificateSecurityState= */ null,
    /* safetyTipsInfo= */ null, /* securityStateIssueIds= */ []);
  TestRunner.mainTarget.model(Security.SecurityModel.SecurityModel).dispatchEventToListeners(
    Security.SecurityModel.Events.VisibleSecurityStateChanged, pageVisibleSecurityState);
  const request = new SDK.NetworkRequest.NetworkRequest(0, 'http://foo.test', 'https://foo.test', 0, 0, null);
  request.setBlockedReason(Protocol.Network.BlockedReason.MixedContent);
  request.mixedContentType = 'blockable';
  SecurityTestRunner.dispatchRequestFinished(request);
  const securityPanel = Security.SecurityPanel.SecurityPanel.instance();
  await AxeCoreTestRunner.runValidation(securityPanel.mainView.contentElement);

  TestRunner.completeTest();
})();
