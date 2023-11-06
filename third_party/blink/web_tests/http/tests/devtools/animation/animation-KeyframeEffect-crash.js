// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(
      `Tests that animations can be created with KeyframeEffect without crashing.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="node" style="background-color: red; height: 100px"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function startAnimationWithKeyframeEffect()
      {
          var effect = new KeyframeEffect(node, { opacity : [ 0, 0.9 ] }, 1000);
          var anim = node.animate(null);
          anim.effect = effect;
      }
  `);

  await UI.ViewManager.ViewManager.instance().showView('animations');
  TestRunner.evaluateInPage('startAnimationWithKeyframeEffect()');
  ElementsTestRunner.waitForAnimationAdded(step2);

  function step2(group) {
    TestRunner.completeTest();
  }
})();
