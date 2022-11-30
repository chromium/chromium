// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of style recalc invalidator invalidations.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <!DOCTYPE HTML>
      <style>
          #testElementFour { color: yellow; }
          #testElementFive { color: teal; }
          #testElementFour:hover { color: azure; }
          #testElementFive:hover { color: cornsilk; }
          #testElementFour .dummy { }
          #testElementFive .dummy { }
          #testElementFour[dir] .dummy {}

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
    `);
  await TestRunner.evaluateInPagePromise(`
      function changeClassNameAndDisplay()
      {
          document.getElementById("testElementOne").className = "red";
          document.getElementById("testElementTwo").className = "red";
          var forceStyleRecalc = document.body.offsetTop;
          return waitForFrame();
      }

      function changeIdWithoutStyleChangeAndDisplay()
      {
          document.getElementById("testElementOne").id = "testElementNoMatchingStyles1";
          document.getElementById("testElementTwo").id = "testElementNoMatchingStyles2";
          var forceStyleRecalc = document.body.offsetTop;
          return waitForFrame();
      }

      function changeIdAndDisplay()
      {
          document.getElementById("testElementNoMatchingStyles1").id = "testElementFour";
          document.getElementById("testElementNoMatchingStyles2").id = "testElementFive";
          var forceStyleRecalc = document.body.offsetTop;
          return waitForFrame();
      }

      function changeStyleAttributeAndDisplay()
      {
          document.getElementById("testElementFour").setAttribute("style", "color: purple");
          document.getElementById("testElementFive").setAttribute("style", "color: pink");
          var forceStyleRecalc = document.body.offsetTop;
          return waitForFrame();
      }

      function changeAttributeAndDisplay()
      {
          document.getElementById("testElementFour").setAttribute("dir", "rtl");
          document.getElementById("testElementFive").setAttribute("dir", "rtl");
          var forceStyleRecalc = document.body.offsetTop;
          return waitForFrame();
      }

      function changePseudoAndDisplay()
      {
          var element1 = document.getElementById("testElementFour");
          var element2 = document.getElementById("testElementFive");
          eventSender.mouseMoveTo(element2.offsetLeft + 2, element2.offsetTop + 2);
          return waitForFrame().then(function() {
              var forceStyleRecalc = document.body.offsetTop;
              return waitForFrame();
          });
      }
  `);

  Root.Runtime.experiments.enableForTest('timelineInvalidationTracking');

  TestRunner.runTestSuite([
    async function testClassName(next) {
      await PerformanceTestRunner.invokeAsyncWithTimeline('changeClassNameAndDisplay');
      PerformanceTestRunner.dumpInvalidations(TimelineModel.TimelineModel.RecordType.UpdateLayoutTree, 0);
      next();
    },

    async function testIdWithoutStyleChange(next) {
      await PerformanceTestRunner.invokeAsyncWithTimeline('changeIdWithoutStyleChangeAndDisplay');
      var event = PerformanceTestRunner.findTimelineEvent(TimelineModel.TimelineModel.RecordType.UpdateLayoutTree);
      TestRunner.assertTrue(!event, 'There should be no style recalculation for an id change without style changes.');
      next();
    },

    async function testId(next) {
      await PerformanceTestRunner.invokeAsyncWithTimeline('changeIdAndDisplay');
      PerformanceTestRunner.dumpInvalidations(TimelineModel.TimelineModel.RecordType.UpdateLayoutTree, 0);
      next();
    },

    async function testStyleAttributeChange(next) {
      await PerformanceTestRunner.invokeAsyncWithTimeline('changeStyleAttributeAndDisplay');
      PerformanceTestRunner.dumpInvalidations(TimelineModel.TimelineModel.RecordType.UpdateLayoutTree, 0);
      next();
    },

    async function testAttributeChange(next) {
      await PerformanceTestRunner.invokeAsyncWithTimeline('changeAttributeAndDisplay');
      PerformanceTestRunner.dumpInvalidations(TimelineModel.TimelineModel.RecordType.UpdateLayoutTree, 0);
      next();
    },

    async function testPseudoChange(next) {
      await PerformanceTestRunner.invokeAsyncWithTimeline('changePseudoAndDisplay');
      PerformanceTestRunner.dumpInvalidations(TimelineModel.TimelineModel.RecordType.UpdateLayoutTree, 0);
      next();
    }
  ]);
})();
