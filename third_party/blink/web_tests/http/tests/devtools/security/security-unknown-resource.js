// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that requests to unresolved origins result in unknown security state and show up in the sidebar origin list.\n`);
  await TestRunner.loadTestModule('security_test_runner');
  await TestRunner.showPanel('security');

  var request = SDK.NetworkRequest.create(
      0, 'http://unknown', 'https://foo.test', 0, 0, null);
  SecurityTestRunner.dispatchRequestFinished(request);

  SecurityTestRunner.dumpSecurityPanelSidebarOrigins();

  TestRunner.completeTest();
})();
