// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that adopted portal is rendered inline correctly.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');

  // Save time on style updates.
  ElementsTestRunner.ignoreSidebarUpdates();

  await TestRunner.navigatePromise('resources/append-predecessor-host.html');

  TestRunner.runTestSuite([
    function testSetUp(next) {
      ElementsTestRunner.expandElementsTree(() => {
        ElementsTestRunner.dumpElementsTree();
        TestRunner.evaluateInPage('activate()');
        TestRunner
            .waitForEvent(
                Host.InspectorFrontendHostAPI.Events.ReattachRootTarget,
                Host.InspectorFrontendHost.events)
            .then(next);
      });
    },

    function testAfterActivate() {
      ElementsTestRunner.expandElementsTree(() => {
        ElementsTestRunner.dumpElementsTree();
        TestRunner.completeTest();
      });
    },
  ]);
})();
