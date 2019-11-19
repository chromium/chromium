// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests EventSource resource type and content.\n`);

  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  await TestRunner.evaluateInPagePromise(`
    function output(message) {
      if (!self._output)
          self._output = [];
      self._output.push('[page] ' + message);
    }

    function receiveEvent()
    {
        var callback;
        var promise = new Promise((fulfill) => callback = fulfill);
        var es = new EventSource("resources/event-stream.asis");
        es.onmessage = onMessage;
        es.onerror = onError;
        return promise;
        function onMessage(data)
        {
            output("got event: " + event.data);
        }

        function onError()
        {
            es.close();
            callback();
        }
    }
  `);

  NetworkTestRunner.recordNetwork();
  TestRunner.callFunctionInPageAsync('receiveEvent').then(step2);

  async function step2() {
    const output = await TestRunner.evaluateInPageAsync('JSON.stringify(self._output)');
    TestRunner.addResults(JSON.parse(output));
    var request1 = NetworkTestRunner.networkRequests().pop();
    TestRunner.addResult(request1.url());
    TestRunner.addResult('resource.type: ' + request1.resourceType());
    TestRunner.assertTrue(!request1.failed, 'Resource loading failed.');

    var { content, error, isEncoded } = await request1.requestContent();
    TestRunner.addResult('resource.content after requesting content: ' + content);
    TestRunner.completeTest();
  }
})();
