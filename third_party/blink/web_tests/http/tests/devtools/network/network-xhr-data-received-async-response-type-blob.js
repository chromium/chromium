// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that dataReceived is called on NetworkDispatcher for XHR with responseType="blob".\n`);
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('network');

  TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'dataReceived', dataReceived);

  NetworkTestRunner.recordNetwork();
  NetworkTestRunner.makeXHR(
      'GET', 'resources/resource.php', true, undefined, undefined, [], false, undefined, 'blob', function() {});

  function dataReceived(event) {
    var request = NetworkTestRunner.networkLog().requestByManagerAndId(
        TestRunner.networkManager, event.requestId);
    if (/resource\.php/.exec(request.url())) {
      TestRunner.addResult('Received data for resource.php');
      TestRunner.addResult('SUCCESS');
      TestRunner.completeTest();
    }
  }
})();
