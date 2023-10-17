// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';
import * as Animation from 'devtools/panels/animation/animation.js';

(async function() {
  TestRunner.addResult(`Tests that animation with negative start delay gets added.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="node" style="background-color: red; height: 100px"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function startAnimation()
      {
          node.animate([{ width: "100px" }, { width: "200px" }], { duration: 200, delay: 100 });
      }

      function startAnimationWithNegativeStartTime()
      {
          animation = node.animate([{ width: "100px" }, { width: "200px" }], { duration: 20000, delay: 100, endDelay: 200 });
          animation.startTime = -10000;
      }
  `);

  // Override timeline width for testing
  Animation.AnimationTimeline.AnimationTimeline.prototype.width = function() {
    return 50;
  };

  await UI.ViewManager.ViewManager.instance().showView('animations');
  var timeline = Animation.AnimationTimeline.AnimationTimeline.instance();
  TestRunner.evaluateInPage('startAnimation()');
  ElementsTestRunner.waitForAnimationAdded(step2);

  function step2(group) {
    TestRunner.addResult(timeline.groupBuffer.indexOf(group) !== -1);
    ElementsTestRunner.waitForAnimationAdded(step3);
    TestRunner.evaluateInPage('startAnimationWithNegativeStartTime()');
  }

  function step3(group) {
    TestRunner.addResult(timeline.groupBuffer.indexOf(group) !== -1);
    TestRunner.completeTest();
  }
})();
