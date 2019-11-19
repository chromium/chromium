// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that after disabling network domain, content saved on backend is removed. https://bugs.webkit.org/show_bug.cgi?id=67995`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  NetworkTestRunner.recordNetwork();
  NetworkTestRunner.makeSimpleXHR('GET', 'resources/resource.php', true, step2);

  async function step2() {
    await TestRunner.NetworkAgent.disable();
    var request1 = NetworkTestRunner.networkRequests().pop();
    request1.requestContent().then(step4);
  }

  function step4({ content, error, isEncoded }) {
    TestRunner.addResult('resource.content after disabling network domain: ' + content);
    TestRunner.NetworkAgent.enable().then(step5);
  }

  function step5() {
    TestRunner.completeTest();
  }
})();
