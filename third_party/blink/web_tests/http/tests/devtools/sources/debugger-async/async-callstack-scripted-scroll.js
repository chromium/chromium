// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests asynchronous call stacks for scripted scroll events.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <div id="outer" style="width: 100px; height: 100px; overflow:auto">
          <div id="inner" style="width: 200px; height: 200px;"></div>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          setTimeout(timeout, 0);
      }

      function timeout()
      {
          var outer = document.getElementById("outer");
          outer.scrollTop = 0;
          outer.addEventListener("scroll", onScroll1, false);
          outer.addEventListener("scroll", onScroll2, false);
          outer.scrollTop = 40;
          outer.scrollTop = 60;
      }

      function onScroll1()
      {
          var outer = document.getElementById("outer");
          outer.removeEventListener("scroll", onScroll1, false);
          debugger;
      }

      function onScroll2()
      {
          var outer = document.getElementById("outer");
          outer.removeEventListener("scroll", onScroll2, false);
          debugger;
      }
  `);

  var totalDebuggerStatements = 2;
  SourcesTestRunner.runAsyncCallStacksTest(totalDebuggerStatements);
})();
