// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests responses in network tab for two XHRs sent without any delay between them. Bug 91630\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('network');

  function initArgs(method, url, async, payload) {
    var args = {};
    args.method = method;
    args.url = url;
    args.async = async;
    args.payload = payload;
    var jsonArgs = JSON.stringify(args).replace(/\"/g, '\\"');
    return jsonArgs;
  }

  NetworkTestRunner.recordNetwork();
  var jsonArgs1 = initArgs('POST', 'resources/echo-payload.php', true, 'request1');
  var jsonArgs2 = initArgs('POST', 'resources/echo-payload.php', true, 'request2');
  ConsoleTestRunner.addConsoleSniffer(step2);
  TestRunner.evaluateInPage(
      'makeXHRForJSONArguments("' + jsonArgs1 + '"); makeXHRForJSONArguments("' + jsonArgs2 + '")');

  var totalXHRs = 2;
  async function step2(msg) {
    if (msg.messageText.indexOf('XHR loaded') === -1 || (--totalXHRs)) {
      ConsoleTestRunner.addConsoleSniffer(step2);
      return;
    }

    var requests = NetworkTestRunner.networkRequests();
    var request1 = requests[requests.length - 2];
    var request2 = requests[requests.length - 1];
    var request1Content = await request1.requestContent();
    var request2Content = await request2.requestContent();

    TestRunner.addResult('resource1.content: ' + request1Content.content);
    TestRunner.addResult('resource2.content: ' + request2Content.content);
    TestRunner.assertTrue(request1Content.content === 'request1'
        && request2Content.content === 'request2');
    TestRunner.completeTest();
  }
})();
