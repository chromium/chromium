// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SecurityTestRunner} from 'security_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that origins with failed requests are shown correctly in the security panel origins list.\n`);
  await TestRunner.showPanel('security');

  var request1 = SDK.NetworkRequest.NetworkRequest.create(
      0, 'https://foo.test/foo.jpg', 'https://foo.test', 0, 0, null);
  request1.setSecurityState(Protocol.Security.SecurityState.Secure);
  SecurityTestRunner.dispatchRequestFinished(request1);

  var request2 = SDK.NetworkRequest.NetworkRequest.create(
      0, 'https://does-not-resolve.test', 'https://does-not-resolve.test', 0, 0,
      null);
  // Leave the security state unknown.
  SecurityTestRunner.dispatchRequestFinished(request2);

  SecurityTestRunner.dumpSecurityPanelSidebarOrigins();

  TestRunner.completeTest();
})();
