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
  await UI.Widget.Widget.allUpdatesComplete;

  async function testAddLocation() {
    const addLocationButton = locationsWidget.contentElement.querySelector('.add-locations-button');
    addLocationButton.click();
    await UI.Widget.Widget.allUpdatesComplete;

    const dialogContent =
        UI.Dialog.Dialog.getInstance()?.contentElement.querySelector(
            '.location-dialog-content');
    TestRunner.addResult(`Opened input box: ${Boolean(dialogContent)}`);

    await AxeCoreTestRunner.runValidation(dialogContent);
  }

  async function testNewLocationError() {
    const dialogContent =
        UI.Dialog.Dialog.getInstance()?.contentElement.querySelector(
            '.location-dialog-content');
    const nameInput =
        dialogContent.querySelector('input[placeholder="Location name"]');
    const latitudeInput =
        dialogContent.querySelector('input[placeholder="Latitude"]');
    const longitudeInput =
        dialogContent.querySelector('input[placeholder="Longitude"]');
    const saveButton = dialogContent.querySelector('.save-button');
    let errorMessage;

    TestRunner.addResult(
        `Invalidating the ${nameInput.getAttribute('placeholder')} input`);
    nameInput.value = '';
    saveButton.click();
    await UI.Widget.Widget.allUpdatesComplete;
    errorMessage =
        dialogContent.querySelector('.editor-field-error').textContent.trim();
    TestRunner.addResult(`Error message: ${errorMessage}`);

    TestRunner.addResult(
        `Invalidating the ${latitudeInput.getAttribute('placeholder')} input`);
    nameInput.value = 'location';
    latitudeInput.value = 'a.a';
    saveButton.click();
    await UI.Widget.Widget.allUpdatesComplete;
    errorMessage =
        dialogContent.querySelector('.editor-field-error').textContent.trim();
    TestRunner.addResult(`Error message: ${errorMessage}`);

    TestRunner.addResult(
        `Invalidating the ${longitudeInput.getAttribute('placeholder')} input`);
    latitudeInput.value = '1.1';
    longitudeInput.value = '1a.1';
    saveButton.click();
    await UI.Widget.Widget.allUpdatesComplete;
    errorMessage =
        dialogContent.querySelector('.editor-field-error').textContent.trim();
    TestRunner.addResult(`Error message: ${errorMessage}`);

    await AxeCoreTestRunner.runValidation(dialogContent);
  }

  TestRunner.runAsyncTestSuite([testAddLocation, testNewLocationError]);
})();
