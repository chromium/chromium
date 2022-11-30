// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of a DOM Dispatch (mousedown)\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <div id="testTarget" style="width:400px; height:400px;">
      Test Mouse Target
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function handleMouseDown(event)
      {
          console.timeStamp("Handling mousedown");
      }

      function performActions()
      {
          var target = document.getElementById("testTarget");
          target.addEventListener("mousedown", handleMouseDown, true);
          var rect = target.getBoundingClientRect();

          // Simulate the mouse down over the target to trigger an EventDispatch
          if (window.eventSender) {
              eventSender.mouseMoveTo(rect.left + rect.width / 2, rect.top + rect.height / 2);
              eventSender.mouseDown();
          }
      }
  `);

  PerformanceTestRunner.performActionsAndPrint('performActions()', 'EventDispatch');
})();
