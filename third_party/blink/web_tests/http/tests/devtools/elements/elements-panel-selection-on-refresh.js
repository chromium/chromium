// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as ElementsModule from 'devtools/panels/elements/elements.js';
import {ElementsTestRunner} from 'elements_test_runner';
import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Tests that elements panel preserves selected node on page refresh.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.navigatePromise('resources/elements-panel-selection-on-refresh.html');

  await new Promise(
      resolve => ElementsTestRunner.selectNodeWithId('test-topic', resolve));

  const restoredPromise = TestRunner.addSnifferPromise(
      ElementsModule.ElementsPanel.ElementsPanel.prototype,
      'lastSelectedNodeSelectedForTest');
  await Promise.all([TestRunner.reloadPagePromise(), restoredPromise]);

  // We should have "test-topic" element selected after refresh.
  var selectedElement =
      ElementsTestRunner.firstElementsTreeOutline().selectedTreeElement;
  var nodeName = selectedElement ? selectedElement.node().nodeName() : 'null';
  TestRunner.addResult('Selected element should be \'P\', was: \'' + nodeName +
                       '\'');
  TestRunner.completeTest();
})();
