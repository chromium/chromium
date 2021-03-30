// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test toolbar state when switching modes.\n`);
  await TestRunner.loadTestModule('device_mode_test_runner');

  var phoneA = DeviceModeTestRunner.buildFakePhone();
  var view = new Emulation.DeviceModeView();
  var toolbar = view._toolbar;
  var model = view._model;
  var viewportSize = new UI.Size(800, 600);
  model.setAvailableSize(viewportSize, viewportSize);

  // Check that default model has type None.
  dumpInfo();

  model.emulate(Emulation.DeviceModeModel.Type.None, null, null);
  dumpType();
  toolbar._switchToResponsive();
  dumpInfo();

  model.emulate(Emulation.DeviceModeModel.Type.None, null, null);
  dumpType();
  toolbar._emulateDevice(phoneA);
  dumpInfo();

  toolbar._switchToResponsive();
  dumpInfo();

  toolbar._emulateDevice(phoneA);
  dumpInfo();

  function dumpType() {
    TestRunner.addResult(`Type: ${model.type()}, Device: ${model.device() ? model.device().title : '<no device>'}`);
  }

  function dumpInfo() {
    dumpType();
    TestRunner.addResult(`Rotate: ${toolbar._modeButton._enabled ? 'enabled': 'disabled'}, Width/Height: ${!toolbar._widthInput.disabled ? 'enabled': 'disabled'}`);
  }

  TestRunner.completeTest();
})();
