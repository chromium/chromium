// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {DeviceModeTestRunner} from 'device_mode_test_runner';

import * as Emulation from 'devtools/panels/emulation/emulation.js';
import * as UIModule from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`Test preservation of orientation and scale when that switching devices in device mode.\n`);

  var phoneA = DeviceModeTestRunner.buildFakePhone();
  var phoneB = DeviceModeTestRunner.buildFakePhone();
  var phoneLarge = DeviceModeTestRunner.buildFakePhone({
    'screen': {
      'horizontal': {'width': 3840, 'height': 720},
      'device-pixel-ratio': 2,
      'vertical': {'width': 720, 'height': 3840}
    }
  });

  var view = new Emulation.DeviceModeView.DeviceModeView();
  var toolbar = view.toolbar;
  var model = view.model;
  var viewportSize = new UIModule.Geometry.Size(800, 600);
  model.setAvailableSize(viewportSize, viewportSize);

  TestRunner.addResult('\nTest that devices automatically zoom to fit.');
  TestRunner.addResult('Switch to phone A');
  toolbar.emulateDevice(phoneA);
  TestRunner.addResult('PhoneA Scale: ' + model.scaleSettingInternal.get());
  TestRunner.addResult('Setting scale to 0.5');
  toolbar.onScaleMenuChanged(0.5);
  TestRunner.addResult('PhoneA Scale: ' + model.scaleSettingInternal.get());
  TestRunner.addResult('Switch to phone B');
  toolbar.emulateDevice(phoneB);
  TestRunner.addResult('PhoneB Scale: ' + model.scaleSettingInternal.get());
  TestRunner.addResult('Switch to phone large');
  toolbar.emulateDevice(phoneLarge);
  TestRunner.addResult('PhoneLarge Scale: ' + model.scaleSettingInternal.get());
  TestRunner.addResult('Rotating...');
  toolbar.modeButton.element.click();
  TestRunner.addResult('PhoneLarge Scale: ' + model.scaleSettingInternal.get());
  TestRunner.addResult('Rotating back...');
  toolbar.modeButton.element.click();
  TestRunner.addResult('PhoneLarge Scale: ' + model.scaleSettingInternal.get());
  TestRunner.addResult('Switch to phone A');
  toolbar.emulateDevice(phoneA);
  TestRunner.addResult('PhoneA Scale: ' + model.scaleSettingInternal.get());

  TestRunner.addResult('\nTurning off auto-zoom.');
  toolbar.autoAdjustScaleSetting.set(false);

  TestRunner.addResult('\nTest that devices do not automatically zoom to fit.');
  TestRunner.addResult('Switch to phone A');
  toolbar.emulateDevice(phoneA);
  TestRunner.addResult('PhoneA Scale: ' + model.scaleSettingInternal.get());
  TestRunner.addResult('Setting scale to 0.75');
  toolbar.onScaleMenuChanged(0.75);
  TestRunner.addResult('PhoneA Scale: ' + model.scaleSettingInternal.get());
  TestRunner.addResult('Switch to phone B');
  toolbar.emulateDevice(phoneB);
  TestRunner.addResult('PhoneB Scale: ' + model.scaleSettingInternal.get());
  TestRunner.addResult('Switch to phone large');
  toolbar.emulateDevice(phoneLarge);
  TestRunner.addResult('PhoneLarge Scale: ' + model.scaleSettingInternal.get());
  TestRunner.addResult('Rotating...');
  toolbar.modeButton.element.click();
  TestRunner.addResult('PhoneLarge Scale: ' + model.scaleSettingInternal.get());
  TestRunner.addResult('Rotating back...');
  toolbar.modeButton.element.click();
  TestRunner.addResult('PhoneLarge Scale: ' + model.scaleSettingInternal.get());
  TestRunner.addResult('Switch to phone A');
  toolbar.emulateDevice(phoneA);
  TestRunner.addResult('PhoneA Scale: ' + model.scaleSettingInternal.get());

  TestRunner.completeTest();
})();
