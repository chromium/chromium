// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of a paint image event\n\n`);
  await TestRunner.loadModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
    function addImage(url, width, height)
    {
        var img = document.createElement('img');
        img.style.position = "absolute";
        img.style.top = "100px";
        img.style.left = "0px";
        img.style.width = width;
        img.style.height = height;
        img.src = url;
        document.body.appendChild(img);
    }

    function addCSSImage(url, width, height)
    {
        var img = document.createElement('div');
        img.style.position = "absolute";
        img.style.top = "100px";
        img.style.left = "100px";
        img.style.width = width;
        img.style.height = height;
        img.style.background = \`url(\${JSON.stringify(url)})\`;
        document.body.appendChild(img);
    }

    function display()
    {
        addImage("../resources/test.png", "40px", "30px");
        addCSSImage("//:0", "30px", "20px"); // should be ignored, see https://crbug.com/776940
        addCSSImage("../resources/test.png", "30px", "20px");
        return waitForFrame();
    }
  `);

  await PerformanceTestRunner.invokeAsyncWithTimeline('display');

  const events = PerformanceTestRunner.mainTrackEvents()
      .filter(e => e.name === TimelineModel.TimelineModel.RecordType.PaintImage);
      TestRunner.assertEquals(events.length, 2, 'PaintImage records not found');
  events.forEach(e => PerformanceTestRunner.printTraceEventProperties(e));

  TestRunner.completeTest();
})();
