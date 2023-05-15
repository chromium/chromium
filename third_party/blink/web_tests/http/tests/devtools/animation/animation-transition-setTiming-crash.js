// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`This test passes if it does not crash.\n`);
  await TestRunner.loadLegacyModule('elements');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #node {
          transition: left 100s;
          left: 0px;
      }
      </style>
      <div id="node"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function startCSSTransition() {
          // Force style recalcs that will trigger a transition.
          getComputedStyle(node).left;
          node.style.left = "100px";
          getComputedStyle(node).left;
      }
  `);

  await UI.viewManager.showView('animations');
  var timeline = Animation.AnimationTimeline.instance();
  TestRunner.evaluateInPage('startCSSTransition()');
  ElementsTestRunner.waitForAnimationAdded(animationAdded);
  function animationAdded(group) {
    group.animations()[0].setTiming(1, 0);
    TestRunner.completeTest();
  }
})();
