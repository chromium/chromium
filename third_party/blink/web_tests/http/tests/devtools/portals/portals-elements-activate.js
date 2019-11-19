// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that element tree is updated after activation.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');

  // Save time on style updates.
  Elements.StylesSidebarPane.prototype.update = function() {};
  Elements.MetricsSidebarPane.prototype.update = function() {};

  await TestRunner.navigatePromise('resources/portal_host.html');

  TestRunner.runTestSuite([
    function testSetUp(next) {
      TestRunner.assertEquals(2, SDK.targetManager.targets().length);
      ElementsTestRunner.expandElementsTree(() => {
        ElementsTestRunner.dumpElementsTree();
        next();
      });
    },

    async function testActivate(next) {
      TestRunner.evaluateInPage(
          'setTimeout(() => {document.querySelector(\'portal\').activate();})');
      const mainTarget = SDK.targetManager.mainTarget();
      await TestRunner.waitForEvent(
          Host.InspectorFrontendHostAPI.Events.ReattachMainTarget,
          Host.InspectorFrontendHost.events);
      next();
    },

    function testAfterActivate(next) {
      TestRunner.assertEquals(1, SDK.targetManager.targets().length);
      ElementsTestRunner.expandElementsTree(() => {
        ElementsTestRunner.dumpElementsTree();
        TestRunner.completeTest();
      });
    },
  ]);
})();
