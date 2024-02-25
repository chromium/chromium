// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Animation from 'devtools/panels/animation/animation.js';

(async function() {
  TestRunner.addResult(
      `Tests the matching performed in AnimationModel of groups composed of transitions, which are applied through a variety of selectors.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      div {
          transition: height 1ms;
          height: 100px;
          width: 100px;
      }

      div.expand {
          height: 200px;
          width: 200px;
      }

      div.duration {
          transition-duration: 2ms !important;
      }

      #node4 {
          transition: all 1ms;
      }
      </style>

      <div id="node1" style="background-color: red"></div>
      <div id="node2" style="background-color: red"></div>
      <div id="node3" style="background-color: red"></div>
      <div id="node4" style="background-color: red"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function startTransition(id)
      {
          document.getElementById(id).style.height = Math.random() * 100 + "px";
      }

      function toggleClass(id, className)
      {
          document.getElementById(id).classList.toggle(className);
      }

      function resetElement(id)
      {
          var element = document.getElementById(id);
          element.style.transitionProperty = "none";
          element.style.width = "100px";
          element.style.height = "100px";
          element.offsetWidth;
          element.style.transitionProperty = "";
          element.style.width = "";
          element.style.height = "";
          element.setAttribute("class", "");
      }
  `);

  var groupIds = [];
  var i = 0;
  var stepNumber = 0;
  var model = TestRunner.mainTarget.model(Animation.AnimationModel.AnimationModel);
  model.ensureEnabled();
  model.addEventListener(Animation.AnimationModel.Events.AnimationGroupStarted, groupStarted);
  // Each step triggers a new transition group.
  var steps = [
    'resetElement(\'node1\'); startTransition(\'node1\')', 'resetElement(\'node2\'); startTransition(\'node2\')',
    'resetElement(\'node3\'); startTransition(\'node3\')',
    'resetElement(\'node1\'); toggleClass(\'node1\', \'duration\'); startTransition(\'node1\')',
    'resetElement(\'node1\'); toggleClass(\'node1\', \'duration\'); startTransition(\'node1\')',
    'resetElement(\'node2\'); toggleClass(\'node2\', \'duration\'); startTransition(\'node2\')',
    'resetElement(\'node1\'); toggleClass(\'node1\', \'expand\')',
    'resetElement(\'node1\'); toggleClass(\'node1\', \'expand\')',
    'resetElement(\'node3\'); toggleClass(\'node3\', \'expand\')',
    'resetElement(\'node4\'); startTransition(\'node4\')',
    'resetElement(\'node4\'); toggleClass(\'node4\', \'expand\')',
    'resetElement(\'node4\'); toggleClass(\'node4\', \'expand\')',
    'resetElement(\'node4\'); toggleClass(\'node4\', \'duration\'); toggleClass(\'node4\', \'expand\')',
    'resetElement(\'node4\'); toggleClass(\'node4\', \'expand\')'
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
