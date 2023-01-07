// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests asynchronous call stacks for window.postMessage.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <div><iframe src="../debugger/resources/post-message-listener.html" id="iframe" width="800" height="100" style="border: 1px solid black;">
      </iframe></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          setTimeout(timeout, 0);
      }

      function timeout()
      {
          window.addEventListener("message", onMessageReceivedInParent, false);
          postMessageToSelf();
          postMessageToFrame("start");
      }

      function onMessageReceivedInParent(e)
      {
          debugger;
          if (/data="start"/.test(e.data || ""))
              postMessageToFrame("done");
      }

      function postMessageToSelf()
      {
          window.postMessage("message to myself", "*");
      }

      function postMessageToFrame(msg)
      {
          var iframe = document.getElementById("iframe");
          var win = iframe.contentWindow;
          win.postMessage(msg, "*");
      }
  `);

  var totalDebuggerStatements = 5;
  SourcesTestRunner.runAsyncCallStacksTest(totalDebuggerStatements);
})();
