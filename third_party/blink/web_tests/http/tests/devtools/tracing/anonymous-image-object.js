// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests the Timeline instrumentation does not crash the renderer upon encountering an anonymous image render object\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <style>
      div.marker::before {
          content: url(resources/test.bmp);
      }
      </style>
      <div id="marker"></div>
    `);
  await TestRunner.addScriptTag('../../../resources/run-after-layout-and-paint.js');
  await TestRunner.evaluateInPagePromise(`
      function doActions()
      {
          var callback;
          var promise = new Promise((fulfill) => callback = fulfill);
          document.getElementById("marker").classList.add("marker");
          var img = document.createElement("img");
          img.src = "resources/test.bmp";
          img.addEventListener("load", onImageLoaded, false);
          function onImageLoaded()
          {
              runAfterLayoutAndPaint(callback);
          }
          return promise;
      }
  `);

  await PerformanceTestRunner.invokeAsyncWithTimeline('doActions');
  TestRunner.addResult('DONE');
  TestRunner.completeTest();
})();
