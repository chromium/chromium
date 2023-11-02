// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of a Layout event\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <style>
      .relayout-boundary {
          overflow: hidden;
          width: 100px;
          height: 100px;
          position: relative;
      }
      </style>
      <body>
      <div class="relayout-boundary">
          <div>text</div>
          <div></div>
          <div>
              <div id="invalidate1"><div>text</div></div>
          </div>
      </div>
      <div class="relayout-boundary">
          <div></div>
          <div>text</div>
          <div id="invalidate2"><div>text</div></div>
          <div></div>
          <div></div>
          <div>text</div>
      </div>
      </body>
    `);
  await TestRunner.evaluateInPagePromise(`
      function invalidateAndForceLayout(element)
      {
          element.style.marginTop = "10px";
          var unused = element.offsetHeight;
      }

      function performActions()
      {
          wrapCallFunctionForTimeline(() => invalidateAndForceLayout(document.getElementById("invalidate1")));
          wrapCallFunctionForTimeline(() => invalidateAndForceLayout(document.getElementById("invalidate2")));
      }
  `);

  PerformanceTestRunner.performActionsAndPrint('performActions()', 'Layout');
})();
