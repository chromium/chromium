// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {DeviceModeTestRunner} from 'device_mode_test_runner';

import * as EmulationModel from 'devtools/models/emulation/emulation.js';
import * as Geometry from 'devtools/models/geometry/geometry.js'

(async function() {
  TestRunner.addResult(`Test that device mode's responsive mode behaves correctly when adjusting inputs.\n`);

  var phone0 = DeviceModeTestRunner.buildFakePhone();
  var phone1 = DeviceModeTestRunner.buildFakePhone();

  var model = EmulationModel.DeviceModeModel.DeviceModeModel.instance();
  var viewportSize = new Geometry.Size(320, 480);
  model.setAvailableSize(viewportSize, viewportSize);

  TestRunner.addResult(
      '\nSetting device mode to responsive mode with viewport of size: ' + JSON.stringify(viewportSize));
  model.emulate(EmulationModel.DeviceModeModel.Type.Responsive, null, null);
  dumpModelInfo();

  var width = viewportSize.width - 1;
  TestRunner.addResult('Setting width to ' + width);
  model.setWidthAndScaleToFit(width);
  dumpModelInfo();

  width = viewportSize.width + 1;
  TestRunner.addResult('Setting width to ' + width);
  model.setWidthAndScaleToFit(width);
  dumpModelInfo();

  TestRunner.addResult('Setting width to ' + viewportSize.width);
  model.setWidthAndScaleToFit(viewportSize.width);
  dumpModelInfo();


  var height = viewportSize.height - 1;
  TestRunner.addResult('Setting height to ' + height);
  model.setHeightAndScaleToFit(height);
  dumpModelInfo();

  height = viewportSize.height + 1;
  TestRunner.addResult('Setting height to ' + height);
  model.setHeightAndScaleToFit(height);
  dumpModelInfo();

  TestRunner.addResult('Setting height to ' + viewportSize.height);
  model.setHeightAndScaleToFit(viewportSize.height);
  dumpModelInfo();


  TestRunner.addResult('\nSetting scale to 0.5');
  model.scaleSetting().set(0.5);
  dumpModelInfo();

  TestRunner.addResult('Setting scale to 1');
  model.scaleSetting().set(1);
  dumpModelInfo();

  TestRunner.addResult('Setting scale to 1.25');
  model.scaleSetting().set(1.25);
  dumpModelInfo();

  TestRunner.completeTest();

  function dumpModelInfo() {
    TestRunner.addResult(
        'Scale: ' + model.scale() + ', appliedDeviceSize: ' + JSON.stringify(model.appliedDeviceSize()) +
        ', screenRect: ' + JSON.stringify(model.screenRect()) + ', visiblePageRect: ' +
        JSON.stringify(model.visiblePageRect()) + ', outlineRect: ' + JSON.stringify(model.outlineRect()));
  }
})();
