// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests the display of animations on the animation timeline.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #node {
          transition: background-color 150ms cubic-bezier(0, 0.5, 0.5, 1);
      }

      #node.css-anim {
          animation: anim 300ms ease-in-out;
      }

      @keyframes anim {
          from {
              width: 100px;
          }
          to {
              width: 200px;
          }
      }
      </style>
      <div id="node" style="background-color: red; height: 100px"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      var animation;

      function startAnimationWithDelay()
      {
          animation = node.animate([{ width: "100px" }, { width: "200px" }], { duration: 200, delay: 100, id: "testId" });
      }

      function startAnimationWithEndDelay()
      {
          animation = node.animate([{ width: "100px" }, { width: "200px" }], { duration: 20000, delay: 100, endDelay: 200, id: "testId2" });
      }

      function startAnimationWithStepTiming()
      {
          animation = node.animate([{ width: "100px", easing: "steps(5, end)" }, { width: "200px", easing: "step-start" }], { duration: 200, id: "testId3" });
      }

      function startCSSAnimation()
      {
          node.classList.add("css-anim");
      }

      function startCSSTransition()
      {
          node.style.backgroundColor = "blue";
      }
  `);

  // Override timeline width for testing
  Animation.AnimationTimeline.prototype.width = function() {
    return 1000;
  };

  await UI.viewManager.showView('animations');
  var timeline = Animation.AnimationTimeline.instance();
  TestRunner.evaluateInPage('startAnimationWithDelay()');
  ElementsTestRunner.waitForAnimationAdded(step2);

  function step2(group) {
    timeline.selectAnimationGroup(group);
    timeline.render();
    TestRunner.addResult('>>>> Animation with start delay only');
    ElementsTestRunner.dumpAnimationTimeline(timeline);
    timeline.reset();
    ElementsTestRunner.waitForAnimationAdded(step3);
    TestRunner.evaluateInPage('startAnimationWithEndDelay()');
  }

  function step3(group) {
    timeline.selectAnimationGroup(group);
    timeline.render();
    TestRunner.addResult('>>>> Animation with start and end delay');
    ElementsTestRunner.dumpAnimationTimeline(timeline);
    ElementsTestRunner.waitForAnimationAdded(step5);
    TestRunner.evaluateInPage('startAnimationWithStepTiming()');
  }

  function step5(group) {
    timeline.selectAnimationGroup(group);
    timeline.render();
    TestRunner.addResult('>>>> Animation with step timing function');
    ElementsTestRunner.dumpAnimationTimeline(timeline);
    timeline.reset();
    ElementsTestRunner.waitForAnimationAdded(step6);
    TestRunner.evaluateInPage('startCSSAnimation()');
  }

  function step6(group) {
    timeline.selectAnimationGroup(group);
    timeline.render();
    TestRunner.addResult('>>>> CSS animation started');
    ElementsTestRunner.dumpAnimationTimeline(timeline);
    timeline.reset();
    ElementsTestRunner.waitForAnimationAdded(step7);
    TestRunner.evaluateInPage('startCSSTransition()');
  }

  function step7(group) {
    timeline.selectAnimationGroup(group);
    timeline.render();
    TestRunner.addResult('>>>> CSS transition started');
    ElementsTestRunner.dumpAnimationTimeline(timeline);
    TestRunner.completeTest();
  }
})();
