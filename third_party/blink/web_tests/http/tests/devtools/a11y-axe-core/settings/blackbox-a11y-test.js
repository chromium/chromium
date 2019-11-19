// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests accessibility in Blackboxing view using the axe-core linter.');

  await TestRunner.loadModule('axe_core_test_runner');
  await UI.viewManager.showView('blackbox');
  const blackboxWidget = await UI.viewManager.view('blackbox').widget();

  async function testAddPattern() {
    const addPatternButton = blackboxWidget._defaultFocusedElement;
    // Make add pattern editor visible
    addPatternButton.click();

    const blackboxInputs = blackboxWidget._list._editor._controls;
    TestRunner.addResult(`Opened input box: ${!!blackboxInputs}`);

    await AxeCoreTestRunner.runValidation(blackboxWidget.contentElement);
  }

  async function testPatternList() {
    blackboxWidget._list.appendItem('test*', true);
    TestRunner.addResult(`Added a pattern in the list: ${blackboxWidget._list._items}`);
    await AxeCoreTestRunner.runValidation(blackboxWidget.contentElement);
  }

  async function testPatternError() {
    const blackboxEditor = blackboxWidget._list._editor;
    const patternInput = blackboxEditor._controls[0];
    // Blur patternInput to run validator
    patternInput.blur();

    const errorMessage = blackboxEditor._errorMessageContainer.textContent;
    TestRunner.addResult(`Error message: ${errorMessage}`);

    await AxeCoreTestRunner.runValidation(blackboxWidget.contentElement);
  }

  TestRunner.runAsyncTestSuite([testAddPattern, testPatternList, testPatternError]);
})();