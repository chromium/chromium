// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Host from 'devtools/core/host/host.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that element tree is updated after activation.\n`);
  await TestRunner.showPanel('elements');

  // Save time on style updates.
  ElementsTestRunner.ignoreSidebarUpdates();

  await TestRunner.navigatePromise('resources/portal_host.html');

  TestRunner.runTestSuite([
    function testSetUp(next) {
      TestRunner.assertEquals(2, SDK.TargetManager.TargetManager.instance().targets().length);
      ElementsTestRunner.expandElementsTree(() => {
        ElementsTestRunner.dumpElementsTree();
        next();
      });
    },

    async function testActivate(next) {
      TestRunner.evaluateInPage(
          'setTimeout(() => {document.querySelector(\'portal\').activate();})');
      const rootTarget = SDK.TargetManager.TargetManager.instance().rootTarget();
      await TestRunner.waitForEvent(
          Host.InspectorFrontendHostAPI.Events.ReattachRootTarget,
          Host.InspectorFrontendHost.InspectorFrontendHostInstance.events);
      next();
    },

    function testAfterActivate(next) {
      TestRunner.assertEquals(1, SDK.TargetManager.TargetManager.instance().targets().length);
      ElementsTestRunner.expandElementsTree(() => {
        ElementsTestRunner.dumpElementsTree();
        TestRunner.completeTest();
      });
    },
  ]);
})();
