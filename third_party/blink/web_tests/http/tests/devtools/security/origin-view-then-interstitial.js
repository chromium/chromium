// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SecurityTestRunner} from 'security_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Security from 'devtools/panels/security/security.js';

(async function() {
  TestRunner.addResult(
      `Tests that the panel transitions to the overview view when navigating to an interstitial. Regression test for https://crbug.com/638601\n`);
  await TestRunner.showPanel('security');

  var request1 = SDK.NetworkRequest.NetworkRequest.create(
      0, 'http://foo.test/', 'http://foo.test', 0, 0, null);
  request1.setSecurityState(Protocol.Security.SecurityState.Insecure);
  SecurityTestRunner.dispatchRequestFinished(request1);

  TestRunner.addResult('Before selecting origin view:');
  TestRunner.dumpDeepInnerHTML(Security.SecurityPanel.SecurityPanel.instance().visibleView.contentElement);

  Security.SecurityPanel.SecurityPanel.instance().sidebarTree.elementsByOrigin.get('http://foo.test').select();

  TestRunner.addResult('Panel on origin view before interstitial:');
  TestRunner.dumpDeepInnerHTML(Security.SecurityPanel.SecurityPanel.instance().visibleView.contentElement);

  // Test that the panel transitions to an origin view when an interstitial is shown. https://crbug.com/559150
  TestRunner.mainTarget.model(SDK.ResourceTreeModel.ResourceTreeModel)
      .dispatchEventToListeners(SDK.ResourceTreeModel.Events.InterstitialShown);
  TestRunner.addResult('After interstitial is shown:');
  TestRunner.dumpDeepInnerHTML(Security.SecurityPanel.SecurityPanel.instance().visibleView.contentElement);

  TestRunner.completeTest();
})();
