// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests event listener breakpoints on XHRs.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          sendXHR();
      }

      function sendXHR()
      {
          var xhr = new XMLHttpRequest();
          xhr.onreadystatechange = function()
          {
              xhr.onreadystatechange = null;
          };
          function downloadEnd()
          {
              xhr.removeEventListener("loadend", downloadEnd, false);
          }
          function uploadEnd()
          {
              xhr.upload.removeEventListener("loadend", uploadEnd, false);
          }
          function downloadProgress()
          {
              xhr.removeEventListener("progress", downloadProgress, false);
          }
          function uploadProgress()
          {
              xhr.upload.removeEventListener("progress", uploadProgress, false);
          }
          function loadCallback()
          {
              xhr.onload = null;
              xhr.onerror = null;
          }
          xhr.onload = loadCallback;
          xhr.onerror = loadCallback;
          xhr.addEventListener("loadend", downloadEnd, false);
          xhr.addEventListener("progress", downloadProgress, false);
          xhr.upload.addEventListener("loadend", uploadEnd, false);
          xhr.upload.addEventListener("progress", uploadProgress, false);
          xhr.open("POST", "/?now=" + Date.now(), true);
          xhr.send(String(sendXHR));
      }
  `);

  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.setEventListenerBreakpoint('listener:load', true, 'xmlhttprequest');
    SourcesTestRunner.setEventListenerBreakpoint('listener:error', true, 'xmlhttprequest');
    SourcesTestRunner.setEventListenerBreakpoint('listener:loadend', true, 'xmlhttprequest');
    SourcesTestRunner.setEventListenerBreakpoint('listener:progress', true, 'xmlhttprequest');
    SourcesTestRunner.setEventListenerBreakpoint('listener:readystatechange', true, 'xmlhttprequest');
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(didPause);
  }

  var totalBreaks = 6;
  var callStacksOutput = [];
  async function didPause(callFrames, reason, breakpointIds, asyncStackTrace, auxData) {
    --totalBreaks;
    auxData = auxData || {};
    var result = await SourcesTestRunner.captureStackTraceIntoString(callFrames) + '\n';
    result += 'Event target: ' + auxData['targetName'] + '\n';

    callStacksOutput.push(result);
    if (totalBreaks) {
      SourcesTestRunner.resumeExecution(SourcesTestRunner.waitUntilPaused.bind(SourcesTestRunner, didPause));
    } else {
      TestRunner.addResult('Captured call stacks in no particular order:');
      callStacksOutput.sort();
      TestRunner.addResults(callStacksOutput);
      completeTest();
    }
  }

  function completeTest() {
    SourcesTestRunner.setEventListenerBreakpoint('listener:load', false, 'xmlhttprequest');
    SourcesTestRunner.setEventListenerBreakpoint('listener:error', false, 'xmlhttprequest');
    SourcesTestRunner.setEventListenerBreakpoint('listener:loadend', false, 'xmlhttprequest');
    SourcesTestRunner.setEventListenerBreakpoint('listener:progress', false, 'xmlhttprequest');
    SourcesTestRunner.setEventListenerBreakpoint('listener:readystatechange', false, 'xmlhttprequest');
    TestRunner.deprecatedRunAfterPendingDispatches(SourcesTestRunner.completeDebuggerTest.bind(SourcesTestRunner));
  }
})();
