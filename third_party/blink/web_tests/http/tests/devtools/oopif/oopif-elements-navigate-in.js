// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that oopif iframes are rendered inline.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');

  // Save time on style updates.
  ElementsTestRunner.ignoreSidebarUpdates();

  await TestRunner.navigatePromise('resources/page-out.html');

  SDK.targetManager.observeTargets({
    targetAdded: async function(target) {
      if (!target.name().startsWith('inner'))
        return;
      target.pageAgent().setLifecycleEventsEnabled(true);
      target.model(SDK.ResourceTreeModel).addEventListener(SDK.ResourceTreeModel.Events.LifecycleEvent, async (event) => {
        if (event.data.name !== 'load')
          return;

        // OOPIF loaded, proceed with the test.
        await ElementsTestRunner.expandAndDump();

        // Navigate iframe to in-process
        let rootTarget = SDK.targetManager.rootTarget();
        await rootTarget.model(SDK.ResourceTreeModel)._agent.setLifecycleEventsEnabled(true);
        TestRunner.evaluateInPagePromise(`document.getElementById('page-iframe').src = 'http://127.0.0.1:8000/devtools/oopif/resources/inner-iframe.html';`);
        rootTarget.model(SDK.ResourceTreeModel).addEventListener(SDK.ResourceTreeModel.Events.LifecycleEvent, async (event) => {
          if (event.data.name === 'load') {
            await ElementsTestRunner.expandAndDump();
            TestRunner.completeTest();
          }
        });
      });
    },

    targetRemoved: function(target) {},
  });
})();
