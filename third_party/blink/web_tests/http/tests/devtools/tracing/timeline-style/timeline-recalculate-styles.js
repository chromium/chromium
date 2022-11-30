// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of a style recalculation event\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
<style>
.test-style {
    color: red;
}
</style>
<section>
  <div></div>
</section>
`);
  await TestRunner.evaluateInPagePromise(`
function forceStyle()
{
  const element = document.createElement("div");
  element.className = "test-style";
  element.innerHTML = "<span>Test data</span>";
  document.querySelector('section > div').appendChild(element);
  var unused = element.offsetWidth;
}

function performActions()
{
  wrapCallFunctionForTimeline(forceStyle);
}
`);

  TestRunner.addResult(`Test data`);
  PerformanceTestRunner.performActionsAndPrint('performActions()', 'UpdateLayoutTree');
})();
