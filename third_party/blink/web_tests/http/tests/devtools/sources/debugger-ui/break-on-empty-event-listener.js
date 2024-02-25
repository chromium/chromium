// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests that scheduled pause is cleared after processing event with empty handler.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <div id="myDiv"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function SHOULD_NOT_STOP_HERE()
      {
          return 239;
      }

      function addEmptyEventListenerAndClick()
      {
          // this event listener won't execute any JS code.
          var div = document.getElementById("myDiv");
          div.addEventListener("click", {});
          div.click();
          SHOULD_NOT_STOP_HERE();
          debugger;
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    var actions = ['Print'];
    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(actions, step2);
    SourcesTestRunner.setEventListenerBreakpoint('listener:click', true);
    TestRunner.evaluateInPageWithTimeout('addEmptyEventListenerAndClick()');
  }

  function step2() {
    SourcesTestRunner.setEventListenerBreakpoint('listener:click', false);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
