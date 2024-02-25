// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult('Tests accessibility in the settings tool locations pane using the axe-core linter.');

  await UI.ViewManager.ViewManager.instance().showView('emulation-locations');
  const locationsWidget = await UI.ViewManager.ViewManager.instance().view('emulation-locations').widget();

  async function testAddLocation() {
    const addLocationButton = locationsWidget.defaultFocusedElement;
    addLocationButton.click();

    const newLocationInputs = locationsWidget.list.editor.controls;
    TestRunner.addResult(`Opened input box: ${Boolean(newLocationInputs)}`);

    await AxeCoreTestRunner.runValidation(locationsWidget.contentElement);
  }

  async function testNewLocationError() {
    const locationsEditor = locationsWidget.list.editor;
    const newLocationInputs = locationsEditor.controls;
    const nameInput = newLocationInputs[0];
    const latitudeInput = newLocationInputs[1];
    const longitudeInput = newLocationInputs[2];
    let errorMessage;

    TestRunner.addResult(`Invalidating the ${nameInput.getAttribute('aria-label')} input`);
    nameInput.dispatchEvent(new Event('input'));
    errorMessage = locationsEditor.errorMessageContainer.textContent;
    TestRunner.addResult(`Error message: ${errorMessage}`);

    TestRunner.addResult(`Invalidating the ${latitudeInput.getAttribute('aria-label')} input`);
    nameInput.value = 'location';
    latitudeInput.value = 'a.a';
    latitudeInput.dispatchEvent(new Event('input'));
    errorMessage = locationsEditor.errorMessageContainer.textContent;
    TestRunner.addResult(`Error message: ${errorMessage}`);

    TestRunner.addResult(`Invalidating the ${longitudeInput.getAttribute('aria-label')} input`);
    latitudeInput.value = '1.1';
    longitudeInput.value = '1a.1';
    longitudeInput.dispatchEvent(new Event('input'));
    errorMessage = locationsEditor.errorMessageContainer.textContent;
    TestRunner.addResult(`Error message: ${errorMessage}`);

    await AxeCoreTestRunner.runValidation(locationsWidget.contentElement);
  }

  TestRunner.runAsyncTestSuite([testAddLocation, testNewLocationError]);
})();
