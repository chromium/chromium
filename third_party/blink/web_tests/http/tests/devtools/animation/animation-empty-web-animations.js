// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';
import * as Animation from 'devtools/panels/animation/animation.js';

(async function() {
  TestRunner.addResult(`Tests the empty web animations do not show up in animation timeline.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="node" style="background-color: red; height: 100px"></div>
    `);

  await UI.ViewManager.ViewManager.instance().showView('animations');
  var timeline = Animation.AnimationTimeline.AnimationTimeline.instance();
  TestRunner.evaluateInPage('document.getElementById("node").animate([], { duration: 200, delay: 100 })');
  TestRunner.addSniffer(Animation.AnimationModel.AnimationModel.prototype, 'animationStarted', animationStarted);

  function animationStarted() {
    TestRunner.addResult(timeline.previewMap.size);
    TestRunner.completeTest();
  }
})();
