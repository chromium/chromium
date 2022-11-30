// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of style recalc events with invalidations.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <!DOCTYPE HTML>
      <div id="testElementOne">PASS</div><div id="testElementTwo">PASS</div><div id="testElementThree">PASS</div>
      <iframe src="../resources/timeline-iframe-paint.html" style="position: absolute; left: 40px; top: 40px; width: 100px; height: 100px; border: none"></iframe>
    `);
  await TestRunner.evaluateInPagePromise(`
      function changeStylesAndDisplay()
      {
          document.getElementById("testElementOne").style.color = "red";
          document.getElementById("testElementTwo").style.color = "blue";
          var forceLayout = document.body.offsetTop;
          return waitForFrame();
      }

      function changeMultipleStylesAndDisplay()
      {
          var elementOne = document.getElementById("testElementOne");
          var elementTwo = document.getElementById("testElementTwo");
          var elementThree = document.getElementById("testElementThree");

          elementOne.style.backgroundColor = "orangered";
          var forceStyleRecalc1 = document.body.offsetTop;
          elementOne.style.color = "mediumvioletred";
          elementTwo.style.color = "deepskyblue";
          var forceStyleRecalc2 = document.body.offsetTop;
          elementOne.style.color = "tomato";
          elementTwo.style.color = "mediumslateblue";
          elementThree.style.color = "mediumspringgreen";
          var forceStyleRecalc3 = document.body.offsetTop;
          return waitForFrame();
      }

      function changeSubframeStylesAndDisplay()
      {
          frames[0].document.body.style.backgroundColor = "papayawhip";
          frames[0].document.body.children[0].style.width = "200px";
          var forceLayout = frames[0].document.body.offsetTop;
          return waitForFrame();
      }
  `);

  Root.Runtime.experiments.enableForTest('timelineInvalidationTracking');

  TestRunner.runTestSuite([
    async function testLocalFrame(next) {
      await PerformanceTestRunner.invokeAsyncWithTimeline('changeStylesAndDisplay');
      PerformanceTestRunner.dumpInvalidations(
          TimelineModel.TimelineModel.RecordType.UpdateLayoutTree, 0, 'first recalc style invalidations');
      next();
    },

    async function multipleStyleRecalcs(next) {
      await PerformanceTestRunner.invokeAsyncWithTimeline('changeMultipleStylesAndDisplay');
      PerformanceTestRunner.dumpInvalidations(
          TimelineModel.TimelineModel.RecordType.UpdateLayoutTree, 0, 'first recalc style invalidations');
      PerformanceTestRunner.dumpInvalidations(
          TimelineModel.TimelineModel.RecordType.UpdateLayoutTree, 1, 'second recalc style invalidations');
      PerformanceTestRunner.dumpInvalidations(
          TimelineModel.TimelineModel.RecordType.UpdateLayoutTree, 2, 'third recalc style invalidations');
      next();
    },

    async function testSubframe(next) {
      await PerformanceTestRunner.invokeAsyncWithTimeline('changeSubframeStylesAndDisplay');
      PerformanceTestRunner.dumpInvalidations(
          TimelineModel.TimelineModel.RecordType.UpdateLayoutTree, 0, 'first recalc style invalidations');
      next();
    }
  ]);
})();
