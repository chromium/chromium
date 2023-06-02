// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of layout events with invalidations.\n`);
  await TestRunner.loadLegacyModule('timeline');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <!DOCTYPE HTML>
      <div id="outerTestElement" style="display: inline-block;"><div id="testElement">PASS</div></div>
      <iframe src="../resources/timeline-iframe-paint.html" style="position: absolute; left: 40px; top: 40px; width: 100px; height: 100px; border: none"></iframe>
    `);
  await TestRunner.evaluateInPagePromise(`
      function display()
      {
          document.getElementById("testElement").style.width = "100px";
          var forceLayout1 = document.body.offsetTop;
          document.getElementById("testElement").style.width = "110px";
          var forceLayout2 = document.body.offsetTop;
          return waitForFrame();
      }

      function updateSubframeAndDisplay()
      {
          frames[0].document.body.children[0].style.width = "10px";
          var forceLayout1 = frames[0].document.body.offsetTop;
          frames[0].document.body.children[0].style.width = "20px";
          var forceLayout2 = frames[0].document.body.offsetTop;
          return waitForFrame();
      }
  `);

  Root.Runtime.experiments.enableForTest('timelineInvalidationTracking');

  TestRunner.runTestSuite([
    async function testLocalFrame(next) {
      await PerformanceTestRunner.invokeAsyncWithTimeline('display');
      PerformanceTestRunner.dumpInvalidations(
          TimelineModel.TimelineModel.RecordType.Layout, 0, 'first layout invalidations');
      PerformanceTestRunner.dumpInvalidations(
          TimelineModel.TimelineModel.RecordType.Layout, 1, 'second layout invalidations');
      next();
    },

    async function testSubframe(next) {
      await PerformanceTestRunner.invokeAsyncWithTimeline('updateSubframeAndDisplay');
      PerformanceTestRunner.dumpInvalidations(
          TimelineModel.TimelineModel.RecordType.Layout, 0, 'first layout invalidations');
      PerformanceTestRunner.dumpInvalidations(
          TimelineModel.TimelineModel.RecordType.Layout, 1, 'second layout invalidations');
      next();
    }
  ]);
})();
