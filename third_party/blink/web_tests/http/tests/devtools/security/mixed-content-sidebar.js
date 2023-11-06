// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SecurityTestRunner} from 'security_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Security from 'devtools/panels/security/security.js';

(async function() {
  TestRunner.addResult(
      `Tests that the sidebar uses the correct styling for mixed content subresources.\n`);
  await TestRunner.showPanel('security');

  const pageVisibleSecurityState = new Security.SecurityModel.PageVisibleSecurityState(
    Protocol.Security.SecurityState.Neutral, null, null,
    ['displayed-mixed-content', 'ran-mixed-content']);
  TestRunner.mainTarget.model(Security.SecurityModel.SecurityModel)
      .dispatchEventToListeners(
        Security.SecurityModel.Events.VisibleSecurityStateChanged,
        pageVisibleSecurityState);

  var passive = SDK.NetworkRequest.NetworkRequest.create(
      0, 'http://foo.test', 'https://foo.test', 0, 0, null);
  passive.mixedContentType = 'optionally-blockable';
  SecurityTestRunner.dispatchRequestFinished(passive);

  var active = SDK.NetworkRequest.NetworkRequest.create(
      0, 'http://bar.test', 'https://bar.test', 0, 0, null);
  active.mixedContentType = 'blockable';
  SecurityTestRunner.dispatchRequestFinished(active);

  TestRunner.addResult('Origin sidebar:');
  TestRunner.dumpDeepInnerHTML(Security.SecurityPanel.SecurityPanel.instance().sidebarTree.element);

  TestRunner.completeTest();
})();
