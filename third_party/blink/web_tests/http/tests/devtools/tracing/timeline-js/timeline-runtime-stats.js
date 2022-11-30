// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Check that RuntimeCallStats are present in profile.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <div id="foo">



      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var div = document.getElementById("foo")
          div.style.width = "10px";
          return div.offsetWidth;
      }
  `);

  Root.Runtime.experiments.enableForTest('timelineV8RuntimeCallStats');
  Root.Runtime.experiments.enableForTest('timelineShowAllEvents');
  await PerformanceTestRunner.evaluateWithTimeline('performActions()');

  var frame = PerformanceTestRunner.mainTrackEvents()
                  .filter(e => e.name === TimelineModel.TimelineModel.RecordType.JSFrame)
                  .map(e => e.args['data']['callFrame'])
                  .find(frame => frame.functionName === 'FunctionCallback' && frame.url === 'native V8Runtime');
  TestRunner.assertTrue(!!frame, 'FunctionCallback frame not found');
  TestRunner.completeTest();
})();
