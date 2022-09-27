// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that InspectorBackendDispatcher is catching incorrect messages.\n`);

  function trimErrorMessage(message) {
    if (message.error && message.error.data) {
      message.error.data = message.error.data.replace(/at position \d+/, "<somewhere>");
    }
    return message;
  }

  var messages = [
    'some wrong string',
    '{}',
    '{"id":"not a number"}',
    '{"id":1,"method":1}',
    '{"id":2,"method":"resourceContent"}',
    '{"id":3,"method":"DOM.test"}',
    '{"id":4,"method":"DOM.requestChildNodes"}',
    '{"id":5,"method":"DOM.requestChildNodes","params":[]}',
    '{"id":6,"method":"DOM.requestChildNodes","params":{}}',
    '{"id":7,"method":"DOM.requestChildNodes","params":{"nodeId":"not a number"}}',
    '{"id":8,"method":"DOM.test"}',
    '{"id":9,"method":"DOM.test" }',
    '{"id":10,"method": "DOM.test" }',
    '{"id":11,"method" : "DOM.test" }',
    '{"id":12, "method" : "DOM.test" }',
    '{"id": 13, "method" : "DOM.test" }',
    '{"id" : 14, "method" : "DOM.test" }',
    '{ "id" : 15, "method" : "DOM.test" }',
    '{  "id"\n :\r 16,\t "method" : "DOM.test" }',
  ];

  var numberOfReports = 0;

  ProtocolClient.InspectorBackend.reportProtocolError = function(error, message) {
    if (numberOfReports < messages.length) {
      TestRunner.addObject(trimErrorMessage(message));
      TestRunner.addResult('-------------------------------------------------------');
    }

    if (++numberOfReports === messages.length)
      TestRunner.completeTest();
  };

  await TestRunner.DebuggerAgent.disable();
  for (var message of messages)
    InspectorFrontendHost.sendMessageToBackend(message);
})();
