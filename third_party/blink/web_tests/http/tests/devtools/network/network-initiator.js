// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests resources initiator for images initiated by IMG tag, static CSS, CSS class added from JavaScript and XHR.\n`);
  await TestRunner.showPanel('network');
  await TestRunner.evaluateInPagePromise(`
      function loadData()
      {
          var iframe = document.createElement("iframe");
          iframe.src = "resources/network-initiator-frame.html";
          document.body.appendChild(iframe);
      }
  `);

  step2();

  function step2() {
    ConsoleTestRunner.addConsoleSniffer(step3, true);
    TestRunner.evaluateInPage('loadData()');
  }

  var expectedDoneMessages = 2;
  async function step3() {
    let messages = await ConsoleTestRunner.dumpConsoleMessagesIntoArray();
    let doneMessagesCount = 0;
    for (let i = 0; i < messages.length; i++) {
      if (messages[i].endsWith("Done.")) {
        doneMessagesCount++;
      }
    }
    if (doneMessagesCount < expectedDoneMessages)
      return;
    function dumpInitiator(url) {
      var matching_requests = NetworkTestRunner.findRequestsByURLPattern(new RegExp(url.replace('.', '\\.')));
      if (matching_requests.length === 0) {
        TestRunner.addResult(url + ' NOT FOUND');
        return;
      }
      var request = matching_requests[0];
      var initiator = request.initiator();
      TestRunner.addResult(request.url() + ': ' + initiator.type);
      if (initiator.url)
        TestRunner.addResult('    ' + initiator.url + ' ' + initiator.lineNumber + ' ' + initiator.columnNumber);
      if (initiator.stack) {
        var stackTrace = initiator.stack;
        for (var i = 0; i < stackTrace.callFrames.length; ++i) {
          var frame = stackTrace.callFrames[i];
          if (frame.lineNumber) {
            TestRunner.addResult('    ' + frame.functionName + ' ' + frame.url + ' ' + frame.lineNumber + ' ' + frame.columnNumber);
            break;
          }
        }
      }
    }

    dumpInitiator('initiator.css');
    dumpInitiator('size=100');
    dumpInitiator('size=200');
    dumpInitiator('size=300');
    dumpInitiator('size=400');
    dumpInitiator('style.css');
    dumpInitiator('empty.html');
    dumpInitiator('module1.js');
    dumpInitiator('module2.js');
    dumpInitiator('example.ttf');
    dumpInitiator('example2.ttf');
    TestRunner.completeTest();
  }
})();
