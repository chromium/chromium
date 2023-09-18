// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as ElementsModule from 'devtools/panels/elements/elements.js';

(async function() {
  TestRunner.addResult(`Tests that adding a new rule works properly with user input.\n`);
  await TestRunner.loadLegacyModule('elements');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected">Text</div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', next);

  async function next() {
    await ElementsModule.StylesSidebarPane.StylesSidebarPane.instance().createNewRuleInViaInspectorStyleSheet();
    eventSender.keyDown('Tab');
    await TestRunner.addSnifferPromise(ElementsModule.StylePropertiesSection.StylePropertiesSection.prototype, 'editingSelectorCommittedForTest');

    TestRunner.addResult('Is editing? ' + UI.isEditing());
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);


    TestRunner.completeTest();
  }
})();
