// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SecurityTestRunner} from 'security_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Security from 'devtools/panels/security/security.js';

(async function() {
  TestRunner.addResult(
      `Tests that the panel shows the correct text for non-cryptographic secure origins\n`);
  await TestRunner.showPanel('security');

  var request1 = SDK.NetworkRequest.NetworkRequest.create(
      0, 'chrome-test://test', 'chrome-test://test', 0, 0, null);
  request1.setSecurityState(Protocol.Security.SecurityState.Secure);
  SecurityTestRunner.dispatchRequestFinished(request1);

  Security.SecurityPanel.SecurityPanel.instance().sidebarTree.elementsByOrigin.get('chrome-test://test').select();

  TestRunner.addResult('Panel on origin view:');
  TestRunner.dumpDeepInnerHTML(Security.SecurityPanel.SecurityPanel.instance().visibleView.contentElement);

  TestRunner.completeTest();
})();
