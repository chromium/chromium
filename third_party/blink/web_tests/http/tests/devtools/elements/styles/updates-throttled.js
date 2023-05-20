// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that Styles sidebar DOM rebuilds are throttled during consecutive updates. Bug 78086.\n`);
  await TestRunner.loadLegacyModule('elements');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected"></div>
    `);

  var UPDATE_COUNT = 5;
  var rebuildCount = 0;

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', selectCallback);
  function selectCallback() {
    TestRunner.addSniffer(Elements.StylesSidebarPane.prototype, 'innerRebuildUpdate', sniffRebuild, true);
    var stylesPane = UI.panels.elements.stylesWidget;
    for (var i = 0; i < UPDATE_COUNT; ++i)
      UI.context.setFlavor(SDK.DOMNode, stylesPane.node());

    TestRunner.deprecatedRunAfterPendingDispatches(completeCallback);
  }

  function completeCallback() {
    if (rebuildCount >= UPDATE_COUNT)
      TestRunner.addResult('ERROR: got ' + rebuildCount + ' rebuilds for ' + UPDATE_COUNT + ' consecutive updates');
    else
      TestRunner.addResult('OK: rebuilds throttled');
    TestRunner.completeTest();
  }

  function sniffRebuild() {
    ++rebuildCount;
  }
})();
