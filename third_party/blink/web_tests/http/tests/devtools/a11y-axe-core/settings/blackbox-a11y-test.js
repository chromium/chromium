// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult('Tests accessibility in IgnoreList view using the axe-core linter.');

  await UI.ViewManager.ViewManager.instance().showView('blackbox');
  const ignoreListWidget = await UI.ViewManager.ViewManager.instance().view('blackbox').widget();

  async function testAddPattern() {
    const addPatternButton = ignoreListWidget.defaultFocusedElement;
    // Make add pattern editor visible
    addPatternButton.click();

    const ignoreListInputs = ignoreListWidget.list.editor.controls;
    TestRunner.addResult(`Opened input box: ${Boolean(ignoreListInputs)}`);

    await AxeCoreTestRunner.runValidation(ignoreListWidget.contentElement);
  }

  async function testPatternList() {
    ignoreListWidget.list.appendItem({pattern: 'test*'}, true);
    TestRunner.addResult(`Added a pattern in the list: ${ignoreListWidget.list.items.map(x => x.pattern).join(',')}`);
    await AxeCoreTestRunner.runValidation(ignoreListWidget.contentElement);
  }

  async function testPatternError() {
    const ignoreListEditor = ignoreListWidget.list.editor;
    const patternInput = ignoreListEditor.controls[0];
    // Send input event to patternInput to run validator
    patternInput.dispatchEvent(new Event('input'));

    const errorMessage = ignoreListEditor.errorMessageContainer.textContent;
    TestRunner.addResult(`Error message: ${errorMessage}`);

    await AxeCoreTestRunner.runValidation(ignoreListWidget.contentElement);
  }

  TestRunner.runAsyncTestSuite([testAddPattern, testPatternList, testPatternError]);
})();
