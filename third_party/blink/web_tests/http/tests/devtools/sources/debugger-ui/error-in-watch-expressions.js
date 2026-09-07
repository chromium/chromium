// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from 'devtools/core/common/common.js';
import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Sources from 'devtools/panels/sources/sources.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Tests that watches pane renders errors in red.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      var foo = 123
  `);

  var watchExpressionsPane = Sources.WatchExpressionsSidebarPane.WatchExpressionsSidebarPane.instance();
  await Sources.SourcesPanel.SourcesPanel.instance().sidebarPaneStack.showView(
      Sources.SourcesPanel.SourcesPanel.instance().watchSidebarPane);

  if (typeof watchExpressionsPane.createWatchExpression === 'function') {
    watchExpressionsPane.performUpdate();
    watchExpressionsPane.createWatchExpression('#$%');
    watchExpressionsPane.saveExpressions();
  } else {
    const setting = Common.Settings.Settings.instance().createLocalSetting(
        'watch-expressions', []);
    setting.set(['#$%']);
    UI.Context.Context.instance().setFlavor(
        SDK.RuntimeModel.ExecutionContext,
        UI.Context.Context.instance().flavor(
            SDK.RuntimeModel.ExecutionContext));
  }
  await UI.Widget.Widget.allUpdatesComplete;
  await new Promise(resolve => setTimeout(resolve, 0));
  await UI.Widget.Widget.allUpdatesComplete;

  TestRunner.addResult(
      watchExpressionsPane.contentElement.deepTextContent().indexOf(
          '<not available>') !== -1 ?
          'SUCCESS' :
          'FAILED');

  // Clear watch expressions after execution.
  if (typeof watchExpressionsPane.deleteAllButtonClicked === 'function') {
    watchExpressionsPane.deleteAllButtonClicked();
  } else {
    Common.Settings.Settings.instance()
        .createLocalSetting('watch-expressions', [])
        .set([]);
  }
  TestRunner.completeTest();
})();
