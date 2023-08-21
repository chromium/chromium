// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that inspect request works for nested OOPIF elements.\n`);
  await TestRunner.loadLegacyModule('elements');
  await TestRunner.showPanel('elements');

  // Save time on style updates.
  ElementsTestRunner.ignoreSidebarUpdates();

  TestRunner.navigatePromise('resources/page-inspect.html');

  SDK.TargetManager.TargetManager.instance().observeTargets({
    targetAdded: async function(target) {
      if (target === SDK.TargetManager.TargetManager.instance().rootTarget() || target === SDK.TargetManager.TargetManager.instance().primaryPageTarget())
        return;
      let complete = false;
      target.pageAgent().setLifecycleEventsEnabled(true);
      target.model(SDK.ResourceTreeModel.ResourceTreeModel).addEventListener(SDK.ResourceTreeModel.Events.LifecycleEvent, async (event) => {
        if (event.data.name === 'load' && !complete) {
          complete = true;

          target.model(SDK.RuntimeModel.RuntimeModel).defaultExecutionContext().evaluate({
            expression: 'inspect(document.body)',
            includeCommandLineAPI: true
          }, false, false);

          UI.context.addFlavorChangeListener(SDK.DOMModel.DOMNode, (event) => {
            const treeOutline = Elements.ElementsTreeOutline.forDOMModel(event.data.domModel());
            TestRunner.addResult(`Selected node has text: ${treeOutline.selectedDOMNode().children()[0].nodeName()}`);
            TestRunner.completeTest();
          });
        }
      });
    },

    targetRemoved: function(target) {},
  });

})();
