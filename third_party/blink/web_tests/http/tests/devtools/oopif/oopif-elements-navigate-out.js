// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that oopif iframes are rendered inline.\n`);
  await TestRunner.showPanel('elements');

  // Save time on style updates.
  ElementsTestRunner.ignoreSidebarUpdates();

  await TestRunner.navigatePromise('resources/page-in.html');

  await ElementsTestRunner.expandAndDump();

  TestRunner.evaluateInPagePromise(`document.getElementById('page-iframe').src = 'http://devtools.oopif.test:8000/devtools/oopif/resources/inner-iframe.html';`);

  SDK.TargetManager.TargetManager.instance().observeTargets({
    targetAdded: async function(target) {
      if (!target.model(SDK.ResourceTreeModel.ResourceTreeModel)) return;
      target.model(SDK.ResourceTreeModel.ResourceTreeModel).agent.setLifecycleEventsEnabled(true);
      let loadedModels = 0;
      target.model(SDK.ResourceTreeModel.ResourceTreeModel).addEventListener(SDK.ResourceTreeModel.Events.LifecycleEvent, async (event) => {
        if (event.data.name === 'load') {
          loadedModels++;

          if (loadedModels >= 2) {
            ElementsTestRunner.expandElementsTree(async () => {
              // Because of the out-of-process component, there is a slight delay here
              // This requires expanding twice.
              await timeout(200);
              await ElementsTestRunner.expandAndDump();
              TestRunner.completeTest();
            });
          }
        }
      });
    },

    targetRemoved: function(target) {},
  });
})();

function timeout(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}
