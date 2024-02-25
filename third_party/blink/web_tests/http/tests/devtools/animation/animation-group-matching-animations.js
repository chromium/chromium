// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Animation from 'devtools/panels/animation/animation.js';

(async function() {
  TestRunner.addResult(
      `Tests the matching performed in AnimationModel of groups composed of animations, which are applied through a variety of selectors.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      div {
          animation-duration: 1ms;
      }

      .long {
          animation-duration: 2ms;
      }

      .expandWidth {
          animation-name: expandWidthAnim;
      }

      .expand {
          animation-name: expandWidthAnim, expandHeightAnim !important;
      }

      @keyframes expandWidthAnim {
          from {
              width: 100px;
          }
          to {
              width: 200px;
          }
      }

      @keyframes expandHeightAnim {
          from {
              height: 100px;
          }
          to {
              height: 200px;
          }
      }
      </style>

      <div id="node1" style="background-color: red; height: 100px"></div>
      <div id="node2" style="background-color: red; height: 100px"></div>
      <div id="node3" style="background-color: red; height: 100px"></div>
      <div id="node4" style="background-color: red; height: 100px"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function restartAnimation(id, name)
      {
          var element = document.getElementById(id);
          element.classList.remove(name);
          // Force style recalc.
          element.offsetHeight;
          element.classList.add(name);
      }

      function toggleClass(id, name, enabled)
      {
          document.getElementById(id).classList.toggle(name, enabled);
      }
  `);

  var groupIds = [];
  var i = 0;
  var stepNumber = 0;
  var model = TestRunner.mainTarget.model(Animation.AnimationModel.AnimationModel);
  model.ensureEnabled();
  model.addEventListener(Animation.AnimationModel.Events.AnimationGroupStarted, groupStarted);
  // Each step triggers a new animation group.
  var steps = [
    'restartAnimation(\'node1\', \'expandWidth\')',
    'restartAnimation(\'node2\', \'expandWidth\')',
    'toggleClass(\'node1\', \'expandWidth\', false); restartAnimation(\'node1\', \'expand\')',
    'restartAnimation(\'node3\', \'expand\')',
    'restartAnimation(\'node3\', \'expand\')',
    'restartAnimation(\'node2\', \'expand\')',  // expandWidthAnim doesn't restart, new group.
    'toggleClass(\'node1\', \'long\', true); restartAnimation(\'node1\', \'expand\')',
    'toggleClass(\'node3\', \'long\', true); restartAnimation(\'node3\', \'expand\')',
    'toggleClass(\'node2\', \'long\', true); restartAnimation(\'node2\', \'expand\')',
    'toggleClass(\'node2\', \'expandWidth\', false); toggleClass(\'node2\', \'long\', true); restartAnimation(\'node2\', \'expand\')',

  ];
  TestRunner.evaluateInPage(steps[0]);

  function groupStarted(event) {
    TestRunner.addResult('>> ' + steps[stepNumber]);
    var group = event.data;

    if (groupIds.indexOf(group.id()) !== -1) {
      TestRunner.addResult('Group #' + groupIds.indexOf(group.id()) + ' started again!\n');
    } else {
      TestRunner.addResult('New group #' + groupIds.length + ' started.\n');
      groupIds.push(group.id());
    }
    stepNumber++;
    if (stepNumber < steps.length)
      TestRunner.evaluateInPage(steps[stepNumber]);
    else
      TestRunner.completeTest();
  }
})();
