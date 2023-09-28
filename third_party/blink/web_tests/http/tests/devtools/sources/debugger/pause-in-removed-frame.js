// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests pause functionality in detached frame.`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(
      `<iframe id="child" src="resources/child.html"></iframe>`);
  await TestRunner.evaluateInPagePromise(`
      window.removeIframe = function()
      {
        var child = document.getElementById('child');
        child.parentNode.removeChild(child);
        debugger;
      };

      function testFunction()
      {
          var childWindow = document.getElementById("child").contentWindow;
          childWindow.sendRequest();
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  async function step2(callFrames) {
    await SourcesTestRunner.captureStackTrace(callFrames);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
