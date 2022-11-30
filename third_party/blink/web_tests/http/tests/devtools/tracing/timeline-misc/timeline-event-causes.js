// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test that causes are correctly generated for various types of events.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.loadLegacyModule('components');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <div id="testElement"></div>
    `);

  function checkStringContains(string, contains) {
    var doesContain = string.indexOf(contains) >= 0;
    TestRunner.check(doesContain, contains + ' should be present in ' + string);
    TestRunner.addResult('PASS - record contained ' + contains);
  }

  TestRunner.runTestSuite([
    async function testTimerInstall(next) {
      function setTimeoutFunction() {
        return new Promise((fulfill) => setTimeout(fulfill, 0));
      }

      var source = setTimeoutFunction.toString();
      source += '\n//# sourceURL=setTimeoutFunction.js';
      TestRunner.evaluateInPage(source);

      await PerformanceTestRunner.invokeAsyncWithTimeline('setTimeoutFunction');

      var linkifier = new Components.Linkifier();
      var event = PerformanceTestRunner.findTimelineEvent('TimerFire');
      TestRunner.check(event, 'Should receive a TimerFire event.');
      var contentHelper = new Timeline.TimelineDetailsContentHelper(
          PerformanceTestRunner.timelineModel().targetByEvent(event), linkifier, true);
      Timeline.TimelineUIUtils.generateCauses(
          event, PerformanceTestRunner.timelineModel().targetByEvent(event), null, contentHelper);
      await TestRunner.waitForPendingLiveLocationUpdates();
      var causes = contentHelper.element.deepTextContent();
      TestRunner.check(causes, 'Should generate causes');
      checkStringContains(causes, 'Timer Installed\n(anonymous) @ setTimeoutFunction.js:');
      next();
    },

    async function testRequestAnimationFrame(next) {
      function requestAnimationFrameFunction(callback) {
        return new Promise((fulfill) => requestAnimationFrame(fulfill));
      }

      var source = requestAnimationFrameFunction.toString();
      source += '\n//# sourceURL=requestAnimationFrameFunction.js';
      TestRunner.evaluateInPage(source);

      await PerformanceTestRunner.invokeAsyncWithTimeline('requestAnimationFrameFunction');
      var linkifier = new Components.Linkifier();
      var event = PerformanceTestRunner.findTimelineEvent('FireAnimationFrame');
      TestRunner.check(event, 'Should receive a FireAnimationFrame event.');
      var contentHelper = new Timeline.TimelineDetailsContentHelper(
          PerformanceTestRunner.timelineModel().targetByEvent(event), linkifier, true);
      Timeline.TimelineUIUtils.generateCauses(
          event, PerformanceTestRunner.timelineModel().targetByEvent(event), null, contentHelper);
      await TestRunner.waitForPendingLiveLocationUpdates();
      var causes = contentHelper.element.deepTextContent();
      TestRunner.check(causes, 'Should generate causes');
      checkStringContains(causes, 'Animation Frame Requested\n(anonymous) @ requestAnimationFrameFunction.js:');
      next();
    },

    async function testStyleRecalc(next) {
      function styleRecalcFunction() {
        var element = document.getElementById('testElement');
        element.style.backgroundColor = 'papayawhip';
        var forceLayout = element.offsetWidth;
      }

      var source = styleRecalcFunction.toString();
      source += '\n//# sourceURL=styleRecalcFunction.js';
      TestRunner.evaluateInPage(source);

      await PerformanceTestRunner.evaluateWithTimeline('styleRecalcFunction()');
      var linkifier = new Components.Linkifier();
      var event = PerformanceTestRunner.findTimelineEvent('UpdateLayoutTree');
      TestRunner.check(event, 'Should receive a UpdateLayoutTree event.');
      var contentHelper = new Timeline.TimelineDetailsContentHelper(
          PerformanceTestRunner.timelineModel().targetByEvent(event), linkifier, true);
      Timeline.TimelineUIUtils.generateCauses(
          event, PerformanceTestRunner.timelineModel().targetByEvent(event), null, contentHelper);
      await TestRunner.waitForPendingLiveLocationUpdates();
      var causes = contentHelper.element.deepTextContent();
      TestRunner.check(causes, 'Should generate causes');
      checkStringContains(causes, 'First Invalidated\nstyleRecalcFunction @ styleRecalcFunction.js:');
      next();
    },

    async function testLayout(next) {
      function layoutFunction() {
        var element = document.getElementById('testElement');
        element.style.width = '200px';
        var forceLayout = element.offsetWidth;
      }

      var source = layoutFunction.toString();
      source += '\n//# sourceURL=layoutFunction.js';
      TestRunner.evaluateInPage(source);

      await PerformanceTestRunner.evaluateWithTimeline('layoutFunction()');
      var linkifier = new Components.Linkifier();
      var event = PerformanceTestRunner.findTimelineEvent('Layout');
      TestRunner.check(event, 'Should receive a Layout event.');
      var contentHelper = new Timeline.TimelineDetailsContentHelper(
          PerformanceTestRunner.timelineModel().targetByEvent(event), linkifier, true);
      Timeline.TimelineUIUtils.generateCauses(
          event, PerformanceTestRunner.timelineModel().targetByEvent(event), null, contentHelper);
      await TestRunner.waitForPendingLiveLocationUpdates();
      var causes = contentHelper.element.deepTextContent();
      TestRunner.check(causes, 'Should generate causes');
      checkStringContains(causes, 'Layout Forced\nlayoutFunction @ layoutFunction.js:');
      checkStringContains(causes, 'First Layout Invalidation\nlayoutFunction @ layoutFunction.js:');
      next();
    }
  ]);
})();
