// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests debugger does not fail when stopped while a panel other than scripts was opened. Both valid and invalid expressions are added to watch expressions.\n`);
  await TestRunner.loadLegacyModule('sources');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          var x = Math.sqrt(16);
          debugger;
          return x;
      }
  `);

  SourcesTestRunner.setQuiet(true);
  Common.settings.createLocalSetting('watchExpressions', []).set([
    'x', 'y.foo'
  ]);
  await SourcesTestRunner.startDebuggerTestPromise();
  UI.panels.sources.sidebarPaneStack.showView(
      UI.panels.sources.watchSidebarPane);
  TestRunner.addResult('Watches before running testFunction:');
  await waitForUpdate();
  TestRunner.evaluateInPagePromise('testFunction()');
  TestRunner.addResult('Watches on pause in testFunction:');
  await waitForUpdate();
  SourcesTestRunner.completeDebuggerTest();

  function waitForUpdate() {
    return new Promise(resolve => {
      TestRunner.addSniffer(
          Sources.WatchExpression.prototype, 'createWatchExpression',
          watchExpressionsUpdated);
      let updateCount = 2;
      function watchExpressionsUpdated(result, wasThrown) {
        if (result !== undefined || wasThrown !== undefined) {
          TestRunner.addResult(this.element.deepTextContent());
          if (--updateCount === 0) {
            resolve();
            return;
          }
        }
        TestRunner.addSniffer(
            Sources.WatchExpression.prototype, 'createWatchExpression',
            watchExpressionsUpdated);
      }
    });
  }
})();
