// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult('Test error message in the settings tool Emulated Device pane');
  await UI.ViewManager.ViewManager.instance().showView('devices');
  const devicesWidget = await UI.ViewManager.ViewManager.instance().view('devices').widget();

  async function testNewDeviceError() {
    const addDeviceButton = devicesWidget.defaultFocusedElement;
    addDeviceButton.click();

    TestRunner.addResult('Invalidating the device pixel ratio');
    const editor = devicesWidget.list.editor;
    const title = editor.control('title');
    const width = editor.control('width');
    const height = editor.control('height');
    const scale = editor.control('scale');
    title.value = 'Device';
    width.value = 700;
    height.value = 400;
    scale.value = '  zzz.213213';

    scale.dispatchEvent(new Event('input'));
    const errorMessage = devicesWidget.list.editor.errorMessageContainer.textContent;
    TestRunner.addResult(`Error message: ${errorMessage}`);
  }

  TestRunner.runAsyncTestSuite([testNewDeviceError]);
})();
