// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of a paint event\n`);
  await TestRunner.loadLegacyModule('timeline');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <iframe src="../resources/timeline-iframe-paint.html" style="position: absolute; left: 40px; top: 40px; width: 100px; height: 100px; border: none"></iframe>
    `);
  await TestRunner.evaluateInPagePromise(`
      function display()
      {
          document.body.style.backgroundColor = "blue";
          return waitForFrame();
      }

      function updateSubframeAndDisplay()
      {
          frames[0].document.body.children[0].style.backgroundColor = "green";
          return waitForFrame();
      }
  `);

  await PerformanceTestRunner.invokeAsyncWithTimeline('display');

  var event = PerformanceTestRunner.findTimelineEvent(TimelineModel.TimelineModel.RecordType.Paint);
  if (event)
    PerformanceTestRunner.printTraceEventProperties(event);
  else
    TestRunner.addResult('FAIL: no paint record found');
  await PerformanceTestRunner.invokeAsyncWithTimeline('updateSubframeAndDisplay');

  var events = PerformanceTestRunner.mainTrackEvents().filter(
      e => e.name === TimelineModel.TimelineModel.RecordType.Paint);
  TestRunner.assertGreaterOrEqual(events.length, 2, 'Paint record with subframe paint not found');
  var topQuad = events[0].args['data'].clip;
  var subframePaint = events[1];
  TestRunner.assertGreaterOrEqual(
      events[0].endTime, subframePaint.endTime, 'Subframe paint is not a child of frame paint');
  var subframeQuad = subframePaint.args['data'].clip;
  TestRunner.assertEquals(8, topQuad.length);
  TestRunner.assertEquals(8, subframeQuad.length);
  TestRunner.assertGreaterOrEqual(subframeQuad[0], topQuad[0]);
  TestRunner.assertGreaterOrEqual(subframeQuad[1], topQuad[1]);
  TestRunner.assertGreaterOrEqual(topQuad[2], subframeQuad[2]);
  TestRunner.assertGreaterOrEqual(subframeQuad[3], topQuad[3]);
  TestRunner.assertGreaterOrEqual(topQuad[4], subframeQuad[4]);
  TestRunner.assertGreaterOrEqual(topQuad[5], subframeQuad[5]);
  TestRunner.assertGreaterOrEqual(subframeQuad[6], topQuad[6]);
  TestRunner.assertGreaterOrEqual(topQuad[7], subframeQuad[7]);

  TestRunner.completeTest();
})();
