// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that adding a new rule works properly with user input.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected">Text</div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', next);

  async function next() {
    await Elements.StylesSidebarPane.instance()._createNewRuleInViaInspectorStyleSheet();
    eventSender.keyDown('Tab');
    await TestRunner.addSnifferPromise(Elements.StylePropertiesSection.prototype, '_editingSelectorCommittedForTest');

    TestRunner.addResult('Is editing? ' + UI.isEditing());
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);


    TestRunner.completeTest();
  }
})();
