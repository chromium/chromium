// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests asynchronous call stacks for XHRs.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      var xhrCount = 0;

      function testFunction()
      {
          setTimeout(timeout, 0);
      }

      function timeout()
      {
          sendSyncXHR();
          sendAsyncXHR();
      }

      function sendAsyncXHR() { sendXHR(true); }
      function sendSyncXHR() { sendXHR(false); }

      function sendXHR(async)
      {
          var xhr = new XMLHttpRequest();
          xhr.onreadystatechange = function()
          {
              if (xhr.readyState == 4) {
                  xhr.onreadystatechange = null;
                  debugger;
              }
          };
          function downloadEnd1()
          {
              xhr.removeEventListener("loadend", downloadEnd1, false);
              debugger;
          }
          function downloadEnd2()
          {
              xhr.removeEventListener("loadend", downloadEnd2, true);
              debugger;
          }
          function uploadEnd()
          {
              xhr.upload.removeEventListener("loadend", uploadEnd, false);
              debugger;
          }
          function downloadProgress()
          {
              debugger;
              xhr.removeEventListener("progress", downloadProgress, false);
          }
          function uploadProgress()
          {
              debugger;
              xhr.upload.removeEventListener("progress", uploadProgress, false);
          }
          xhr.addEventListener("loadend", downloadEnd1, false);
          xhr.addEventListener("loadend", downloadEnd2, true);
          if (async) {
              xhr.upload.addEventListener("loadend", uploadEnd, false);
              xhr.addEventListener("progress", downloadProgress, false);
              xhr.upload.addEventListener("progress", uploadProgress, false);
          }
          xhr.open("POST", "/foo?count=" + (++xhrCount) + "&now=" + Date.now(), async);
          xhr.send(String(sendXHR));
      }
  `);

  var totalDebuggerStatements = 9;
  SourcesTestRunner.runAsyncCallStacksTest(totalDebuggerStatements);
})();
