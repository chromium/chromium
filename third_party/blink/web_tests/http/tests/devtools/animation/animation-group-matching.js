// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Animation from 'devtools/panels/animation/animation.js';

(async function() {
  TestRunner.addResult(`Tests the matching of groups in AnimationModel.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #node {
          transition: height 150ms;
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
      function startCSSTransition()
      {
          node.style.height = Math.random() * 100 + "px";
      }

      function startCSSAnimation()
      {
          node.classList.add("css-anim");
      }
  `);

  var firstGroup;
  var i = 0;
  startTransition();

  function startTransition() {
    var model = TestRunner.mainTarget.model(Animation.AnimationModel.AnimationModel);
    model.ensureEnabled();
    model.addEventListener(Animation.AnimationModel.Events.AnimationGroupStarted, groupStarted);
    TestRunner.evaluateInPage('startCSSTransition()');
  }

  function groupStarted(event) {
    TestRunner.addResult('Animation group triggered');
    TestRunner.addResult('First animation of type: ' + event.data.animations()[0].type());
    var group = event.data;
    if (!firstGroup)
      firstGroup = group;
    TestRunner.addResult('Matches first group: ' + firstGroup.matches(group));
    i++;
    if (i < 5)
      TestRunner.evaluateInPage('startCSSTransition()');
    else if (i < 6)
      TestRunner.evaluateInPage('startCSSAnimation()');
    else
      TestRunner.completeTest();
  }
})();
