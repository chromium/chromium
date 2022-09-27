// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test performance.mark/measure records on Timeline\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function simplePerformanceMeasure()
      {
          performance.mark("a-start");
          performance.mark("a-end");
          performance.measure("a", "a-start", "a-end");
      }

      function doWork()
      {
          for(var i = 0; i < 10000; i++) {}
      }

      function nestedPerformanceMeasure()
      {
          performance.mark("a-start");
          {
              performance.mark("b-start");
              doWork();
              performance.mark("b-end");
              {
                  performance.mark("c-start");
                  {
                      performance.mark("d-start");
                      doWork();
                      performance.mark("d-end");
                  }
                  performance.mark("c-end");
              }
          }
          performance.mark("a-end");
          performance.measure("a", "a-start", "a-end");
          performance.measure("b", "b-start", "b-end");
          performance.measure("c", "c-start", "c-end");
          performance.measure("d", "d-start", "d-end");
      }

      function unbalancedPerformanceMeasure()
      {
          performance.mark("a-start");
          performance.mark("b-start");
          performance.mark("a-end");
          performance.mark("b-end");
          performance.measure("a", "a-start", "a-end");
          performance.measure("b", "b-start", "b-end");
      }

      function unnestedPerformanceMeasure()
      {
          performance.mark("ab-start");
          performance.mark("a-end");
          doWork();
          performance.mark("b-end");
          performance.measure("a", "ab-start", "a-end");
          performance.measure("b", "ab-start", "b-end");
      }

      function parentMeasureIsOnTop()
      {
          performance.mark("startTime1");
          doWork();
          performance.mark("endTime1");

          performance.mark("startTime2");
          doWork();
          performance.mark("endTime2");

          performance.measure("durationTimeTotal", "startTime1", "endTime2");
          performance.measure("durationTime1", "startTime1", "endTime1");
          performance.measure("durationTime2", "startTime2", "endTime2");
      }
  `);

  TestRunner.runTestSuite([
    function testSimplePerformanceMeasure(next) {
      performActions('simplePerformanceMeasure()', next);
    },

    function testNestedPerformanceMeasure(next) {
      performActions('nestedPerformanceMeasure()', next);
    },

    function testUnbalancedPerformanceMeasure(next) {
      performActions('unbalancedPerformanceMeasure()', next);
    },

    function testUnnestedPerformanceMeasure(next) {
      performActions('unnestedPerformanceMeasure()', next);
    },

    function testParentMeasureIsOnTop(next) {
      performActions('parentMeasureIsOnTop()', next);
    }
  ]);

  function dumpUserTimings() {
    var model = PerformanceTestRunner.timelineModel();
    for (const track of model.tracks()) {
      if (track.type !== TimelineModel.TimelineModel.TrackType.Timings) continue;
      for (const event of track.asyncEvents) {
        if (event.hasCategory(TimelineModel.TimelineModel.Category.UserTiming))
          TestRunner.addResult(event.name);
      }
    }
  }

  async function performActions(actions, next) {
    await PerformanceTestRunner.evaluateWithTimeline(actions);
    dumpUserTimings();
    next();
  }
})();
