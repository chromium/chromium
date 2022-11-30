// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that elements panel preserves selected node on page refresh.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.navigatePromise('resources/elements-panel-selection-on-refresh.html');

  ElementsTestRunner.selectNodeWithId('test-topic', step1);

  function step1() {
    TestRunner.reloadPage(step2);
  }

  function step2() {
    TestRunner.deprecatedRunAfterPendingDispatches(step3);
  }

  function step3() {
    // We should have "test-topic" element selected after refresh.
    var selectedElement = ElementsTestRunner.firstElementsTreeOutline().selectedTreeElement;
    var nodeName = selectedElement ? selectedElement.node().nodeName() : 'null';
    TestRunner.addResult('Selected element should be \'P\', was: \'' + nodeName + '\'');
    TestRunner.completeTest();
  }
})();
