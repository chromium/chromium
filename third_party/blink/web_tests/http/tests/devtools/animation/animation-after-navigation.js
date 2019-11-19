// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that animation view works post navigation.\n`);

  // This loads the animations view on the existing page, which is
  // somewhere below http://127.0.0.1:8000/. By loading the animations view,
  // we'll start the InspectorAnimationAgent.
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await UI.viewManager.showView('animations');

  // This navigation starts a new renderer, because it's to localhost as
  // opposed to 127.0.0.1. This means, that within the new renderer, the
  // animation agent must resume. If it doesn't we'll get the symptoms
  // described in http://crbug.com/999066.
  await TestRunner.navigatePromise('http://localhost:8000/')

  // Trigger an animation and observe it. We will only observe it only if the
  // animation agent was restarted after the navigation - otherwise this test
  // will time out.
  await TestRunner.loadHTML(`
      <div id="node" style="background-color: red; height: 100px"></div>
    `);
  var timeline = self.runtime.sharedInstance(Animation.AnimationTimeline);
  TestRunner.addSniffer(Animation.AnimationModel.prototype, 'animationStarted',
      () => {
        TestRunner.addResult('SUCCESS (size = ' +
                             timeline._previewMap.size + ', expecting 1)');
        TestRunner.completeTest();
      });
  TestRunner.evaluateInPage(`
      document.getElementById("node").animate(
        [{ width: "100px" }, { width: "200px" }],
        { duration: 200, delay: 100 })`);
})();
