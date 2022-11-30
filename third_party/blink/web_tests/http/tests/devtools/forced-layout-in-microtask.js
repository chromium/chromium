// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that Layout record has correct locations of layout being invalidated and forced.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <style>
      .test { height: 20px; }
      </style>
      <div id="test"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          return fetch("${TestRunner.url('resources/source1.js')}").then(() => {
              var element = document.getElementById("test");
              element.className = "test";
              var unused = element.offsetHeight;
          });
      }
  `);

  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');
  await PerformanceTestRunner.printTimelineRecordsWithDetails('Layout');
  TestRunner.completeTest();
})();
