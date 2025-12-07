// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Sources from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(
      `Tests debugger does not fail when stopped while a panel other than scripts was opened. Both valid and invalid expressions are added to watch expressions.\n`);
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
  Common.Settings.Settings.instance().createLocalSetting('watch-expressions', []).set([
    'x', 'y.foo'
  ]);
  await SourcesTestRunner.startDebuggerTestPromise();
  Sources.SourcesPanel.SourcesPanel.instance().sidebarPaneStack.showView(
      Sources.SourcesPanel.SourcesPanel.instance().watchSidebarPane);
  TestRunner.addResult('Watches before running testFunction:');
  await waitForUpdate();
  TestRunner.evaluateInPagePromise('testFunction()');
  TestRunner.addResult('Watches on pause in testFunction:');
  await waitForUpdate();
  SourcesTestRunner.completeDebuggerTest();

  function waitForUpdate() {
    return new Promise(resolve => {
      TestRunner.addSniffer(
          Sources.WatchExpressionsSidebarPane.WatchExpression.prototype, 'createWatchExpression',
          watchExpressionsUpdated);
      const watches = [];
      function watchExpressionsUpdated(result, exceptionDetails) {
        if (result !== undefined || exceptionDetails !== undefined) {
          watches.push(this.element.deepTextContent().trim());
          if (watches.length === 2) {
            watches.sort().forEach(TestRunner.addResult);
            resolve();
            return;
          }
        }
        TestRunner.addSniffer(
            Sources.WatchExpressionsSidebarPane.WatchExpression.prototype, 'createWatchExpression',
            watchExpressionsUpdated);
      }
    });
  }
})();
