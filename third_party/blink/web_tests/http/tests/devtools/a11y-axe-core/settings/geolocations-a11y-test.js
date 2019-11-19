// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      'Tests accessibility in the settings tool geolocations pane using the axe-core linter.');

  await TestRunner.loadModule('axe_core_test_runner');
  await UI.viewManager.showView('emulation-geolocations');
  const geolocationsWidget = await UI.viewManager.view('emulation-geolocations').widget();

  async function testAddLocation() {
    const addLocationButton = geolocationsWidget._defaultFocusedElement;
    addLocationButton.click();

    const newLocationInputs = geolocationsWidget._list._editor._controls;
    TestRunner.addResult(`Opened input box: ${!!newLocationInputs}`);

    await AxeCoreTestRunner.runValidation(geolocationsWidget.contentElement);
  }

  async function testNewLocationError() {
    const locationsEditor = geolocationsWidget._list._editor;
    const newLocationInputs = locationsEditor._controls;
    const nameInput = newLocationInputs[0];
    const latitudeInput = newLocationInputs[1];
    const longitudeInput = newLocationInputs[2];
    let errorMessage;

    TestRunner.addResult(`Invalidating the ${nameInput.getAttribute('aria-label')} input`);
    nameInput.blur();
    errorMessage = locationsEditor._errorMessageContainer.textContent;
    TestRunner.addResult(`Error message: ${errorMessage}`);

    TestRunner.addResult(`Invalidating the ${latitudeInput.getAttribute('aria-label')} input`);
    nameInput.value = 'location';
    latitudeInput.value = 'a.a';
    latitudeInput.dispatchEvent(new Event('input'));
    errorMessage = locationsEditor._errorMessageContainer.textContent;
    TestRunner.addResult(`Error message: ${errorMessage}`);

    TestRunner.addResult(`Invalidating the ${longitudeInput.getAttribute('aria-label')} input`);
    latitudeInput.value = '1.1';
    longitudeInput.value = '1a.1';
    longitudeInput.dispatchEvent(new Event('input'));
    errorMessage = locationsEditor._errorMessageContainer.textContent;
    TestRunner.addResult(`Error message: ${errorMessage}`);

    await AxeCoreTestRunner.runValidation(geolocationsWidget.contentElement);
  }

  TestRunner.runAsyncTestSuite([testAddLocation, testNewLocationError]);
})();