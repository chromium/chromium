// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of style recalc events with invalidations.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <!DOCTYPE HTML>
      <style>
          .testHolder > .red { background-color: red; }
          .testHolder > .green { background-color: green; }
          .testHolder > .blue { background-color: blue; }
          .testHolder > .snow { background-color: snow; }
          .testHolder > .red .dummy { }
          .testHolder > .green .dummy { }
          .testHolder > .blue .dummy { }
          .testHolder > .snow .dummy { }
      </style>
      <div class="testHolder">
      <div id="testElementOne">PASS</div><div id="testElementTwo">PASS</div><div id="testElementThree">PASS</div>
      </div>
      <iframe src="../resources/timeline-iframe-with-style.html" style="position: absolute; left: 40px; top: 40px; width: 100px; height: 100px; border: none"></iframe>
    `);
  await TestRunner.evaluateInPagePromise(`
      function changeStylesAndDisplay()
      {
          document.getElementById("testElementOne").className = "red";
          document.getElementById("testElementTwo").className = "red";
          var forceStyleRecalc = document.body.offsetTop;
          return waitForFrame();
      }

      function changeMultipleStylesAndDisplay()
      {
          var elementOne = document.getElementById("testElementOne");
          var elementTwo = document.getElementById("testElementTwo");
          var elementThree = document.getElementById("testElementThree");
          elementOne.className = "green";
          var forceStyleRecalc1 = document.body.offsetTop;
          elementOne.className = "blue";
          elementTwo.className = "blue";
          var forceStyleRecalc2 = document.body.offsetTop;
          elementOne.className = "snow";
          elementTwo.className = "snow";
          elementThree.className = "snow";
          var forceStyleRecalc3 = document.body.offsetTop;
          return waitForFrame();
      }

      function changeMultipleSubframeStylesAndDisplay()
      {
          var innerDocument = frames[0].document;
          var elementOne = innerDocument.getElementById("testElementOne");
          var elementTwo = innerDocument.getElementById("testElementTwo");
          var elementThree = innerDocument.getElementById("testElementThree");
          elementOne.className = "green";
          var forceStyleRecalc1 = innerDocument.body.offsetTop;
          elementOne.className = "blue";
          elementTwo.className = "blue";
          var forceStyleRecalc2 = innerDocument.body.offsetTop;
          elementOne.className = "snow";
          elementTwo.className = "snow";
          elementThree.className = "snow";
          var forceStyleRecalc3 = innerDocument.body.offsetTop;
          return waitForFrame();
      }
  `);

  Root.Runtime.experiments.enableForTest('timelineInvalidationTracking');

  TestRunner.runTestSuite([
    async function testLocalFrame(next) {
      await PerformanceTestRunner.invokeAsyncWithTimeline('changeStylesAndDisplay');
      PerformanceTestRunner.dumpInvalidations(
          TimelineModel.TimelineModel.RecordType.UpdateLayoutTree, 0, 'first recalculate styles');
      next();
    },

    async function multipleStyleRecalcs(next) {
      await PerformanceTestRunner.invokeAsyncWithTimeline('changeMultipleStylesAndDisplay');
      PerformanceTestRunner.dumpInvalidations(
          TimelineModel.TimelineModel.RecordType.UpdateLayoutTree, 0, 'first recalculate styles');
      PerformanceTestRunner.dumpInvalidations(
          TimelineModel.TimelineModel.RecordType.UpdateLayoutTree, 1, 'second recalculate styles');
      PerformanceTestRunner.dumpInvalidations(
          TimelineModel.TimelineModel.RecordType.UpdateLayoutTree, 2, 'third recalculate styles');
      next();
    },

    async function testSubframe(next) {
      await PerformanceTestRunner.invokeAsyncWithTimeline('changeMultipleSubframeStylesAndDisplay');
      PerformanceTestRunner.dumpInvalidations(
          TimelineModel.TimelineModel.RecordType.UpdateLayoutTree, 0, 'first recalculate styles');
      PerformanceTestRunner.dumpInvalidations(
          TimelineModel.TimelineModel.RecordType.UpdateLayoutTree, 1, 'second recalculate styles');
      PerformanceTestRunner.dumpInvalidations(
          TimelineModel.TimelineModel.RecordType.UpdateLayoutTree, 2, 'third recalculate styles');
      next();
    }
  ]);
})();
