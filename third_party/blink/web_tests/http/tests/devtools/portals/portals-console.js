// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that the console works correctly with portals`);
  await TestRunner.loadLegacyModule('console');
  await TestRunner.showPanel('console');

  await TestRunner.navigatePromise('resources/append-predecessor-host.html');

  async function setContextLabel(target, label) {
    var runtimeModel = target.model(SDK.RuntimeModel);
    await TestRunner.waitForExecutionContext(runtimeModel);
    runtimeModel.executionContexts()[0].setLabel(label);
  }

  var targets = SDK.targetManager.targets();
  TestRunner.assertEquals(2, targets.length);

  TestRunner.runTestSuite([
    async function testMainConsole(next) {
      ConsoleTestRunner.selectMainExecutionContext();
      await ConsoleTestRunner.evaluateInConsoleAndDumpPromise(
          '!!window.portalHost');
      await ConsoleTestRunner.evaluateInConsoleAndDumpPromise(
          'window.location.pathname');
      next();
    },

    async function testContextSelector(next) {
      const selector = Console.ConsoleView.instance().consoleContextSelector;
      TestRunner.assertEquals(selector.items.length, 2);
      const executionContext = selector.items.at(1);
      TestRunner.assertEquals(selector.titleFor(executionContext), 'append-predecessor.html');
      TestRunner.assertEquals(selector.depthFor(executionContext), 1);
      next();
    },

    async function testPortalConsole(next) {
      await setContextLabel(targets[1], 'portal');
      ConsoleTestRunner.changeExecutionContext('portal');
      await ConsoleTestRunner.evaluateInConsoleAndDumpPromise(
          '!!window.portalHost', true);
      await ConsoleTestRunner.evaluateInConsoleAndDumpPromise(
          'window.location.pathname', true);
      next();
    },

    async function activate(next) {
      TestRunner.evaluateInPage('activate()');
      await TestRunner.waitForTargetRemoved(SDK.targetManager.rootTarget());
      await TestRunner.waitForTarget();
      await TestRunner.waitForTarget(target => target != SDK.targetManager.rootTarget());
      await TestRunner.waitForExecutionContext(TestRunner.runtimeModel);
      targets = SDK.targetManager.targets();
      next();
    },

    async function testMainConsoleAfterActivation(next) {
      ConsoleTestRunner.selectMainExecutionContext();
      await ConsoleTestRunner.evaluateInConsoleAndDumpPromise(
          '!!window.portalHost');
      await ConsoleTestRunner.evaluateInConsoleAndDumpPromise(
          'window.location.pathname');
      next();
    },

    async function testPortalConsoleAfterActivation() {
      await setContextLabel(targets[1], 'portal');
      ConsoleTestRunner.changeExecutionContext('portal');
      await ConsoleTestRunner.evaluateInConsoleAndDumpPromise(
          '!!window.portalHost', true);
      await ConsoleTestRunner.evaluateInConsoleAndDumpPromise(
          'window.location.pathname', true);
      TestRunner.completeTest();
    },
  ]);
})();
