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
      function invalidateStyle()
      {
          var element = document.getElementById("test");
          element.className = "test";
      }

      function forceLayout()
      {
          var element = document.getElementById("test");
          var unused = element.offsetHeight;
      }

      function performActions()
      {
          invalidateStyle();
          forceLayout();
      }
  `);

  await PerformanceTestRunner.evaluateWithTimeline('performActions()');
  var event = PerformanceTestRunner.findTimelineEvent(TimelineModel.TimelineModel.RecordType.Layout);
  var initiator = TimelineModel.TimelineData.forEvent(event).initiator();
  TestRunner.addResult(
      'layout invalidated: ' + TimelineModel.TimelineData.forEvent(initiator).stackTrace[0].functionName);
  TestRunner.addResult('layout forced: ' + TimelineModel.TimelineData.forEvent(event).stackTrace[0].functionName);
  TestRunner.completeTest();
})();
