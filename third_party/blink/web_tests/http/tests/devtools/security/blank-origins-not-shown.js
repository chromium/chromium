// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that blank origins aren't shown in the security panel origins list.\n`);
  await TestRunner.loadTestModule('security_test_runner');
  await TestRunner.showPanel('security');

  var request1 = SDK.NetworkRequest.create(
      0, 'https://foo.test/foo.jpg', 'https://foo.test', 0, 0, null);
  SecurityTestRunner.dispatchRequestFinished(request1);

  var request2 = SDK.NetworkRequest.create(
      0,
      'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAkAAAAKCAYAAABmBXS+AAAAPElEQVR42mNgQAMZGRn/GfABkIIdO3b8x6kQpgAEsCpEVgADKAqxKcBQCCLwARRFIBodYygiyiSCighhAO4e2jskhrm3AAAAAElFTkSuQmCC',
      'https://foo.test', 0, 0, null);
  SecurityTestRunner.dispatchRequestFinished(request2);

  SecurityTestRunner.dumpSecurityPanelSidebarOrigins();

  TestRunner.completeTest();
})();
