// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that adding a new rule works properly with user input.\n`);
  await TestRunner.loadLegacyModule('elements');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected">Text</div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', next);

  async function next() {
    await Elements.StylesSidebarPane.instance().createNewRuleInViaInspectorStyleSheet();
    eventSender.keyDown('Tab');
    await TestRunner.addSnifferPromise(Elements.StylePropertiesSection.prototype, 'editingSelectorCommittedForTest');

    TestRunner.addResult('Is editing? ' + UI.isEditing());
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);


    TestRunner.completeTest();
  }
})();
