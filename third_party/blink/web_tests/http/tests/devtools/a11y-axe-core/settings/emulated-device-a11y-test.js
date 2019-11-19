// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Test error message in the settings tool Emulated Device pane');
  await UI.viewManager.showView('devices');
  const devicesWidget = await UI.viewManager.view('devices').widget();

  async function testNewDeviceError() {
    const addDeviceButton = devicesWidget._defaultFocusedElement;
    addDeviceButton.click();

    TestRunner.addResult(`Invalidating the device pixel ratio`);
    const editor = devicesWidget._list._editor;
    const title = editor.control('title');
    const width = editor.control('width');
    const height = editor.control('height');
    const scale = editor.control('scale');
    title.value = 'Device';
    width.value = 700;
    height.value = 400;
    scale.value = '  zzz.213213';

    scale.dispatchEvent(new Event('input'));
    const errorMessage = devicesWidget._list._editor._errorMessageContainer.textContent;
    TestRunner.addResult(`Error message: ${errorMessage}`);
  }

  TestRunner.runAsyncTestSuite([testNewDeviceError]);
})();