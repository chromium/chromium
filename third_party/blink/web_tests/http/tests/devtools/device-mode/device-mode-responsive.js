// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {DeviceModeTestRunner} from 'device_mode_test_runner';

import * as Emulation from 'devtools/panels/emulation/emulation.js';
import * as UIModule from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`Test that device mode's responsive mode behaves correctly when adjusting inputs.\n`);

  var phone0 = DeviceModeTestRunner.buildFakePhone();
  var phone1 = DeviceModeTestRunner.buildFakePhone();

  var view = new Emulation.DeviceModeView.DeviceModeView();
  var toolbar = view.toolbar;
  var model = view.model;
  var viewportSize = new UIModule.Geometry.Size(320, 480);
  model.setAvailableSize(viewportSize, viewportSize);

  TestRunner.addResult(
      '\nSetting device mode to responsive mode with viewport of size: ' + JSON.stringify(viewportSize));
  toolbar.switchToResponsive();
  dumpModelInfo();

  var width = viewportSize.width - 1;
  TestRunner.addResult('Setting width to ' + width);
  toolbar.model.setWidthAndScaleToFit(width);
  dumpModelInfo();

  width = viewportSize.width + 1;
  TestRunner.addResult('Setting width to ' + width);
  toolbar.model.setWidthAndScaleToFit(width);
  dumpModelInfo();

  TestRunner.addResult('Setting width to ' + viewportSize.width);
  toolbar.model.setWidthAndScaleToFit(viewportSize.width);
  dumpModelInfo();


  var height = viewportSize.height - 1;
  TestRunner.addResult('Setting height to ' + height);
  toolbar.model.setHeightAndScaleToFit(height);
  dumpModelInfo();

  height = viewportSize.height + 1;
  TestRunner.addResult('Setting height to ' + height);
  toolbar.model.setHeightAndScaleToFit(height);
  dumpModelInfo();

  TestRunner.addResult('Setting height to ' + viewportSize.height);
  toolbar.model.setHeightAndScaleToFit(viewportSize.height);
  dumpModelInfo();


  TestRunner.addResult('\nSetting scale to 0.5');
  toolbar.onScaleMenuChanged(0.5);
  dumpModelInfo();

  TestRunner.addResult('Setting scale to 1');
  toolbar.onScaleMenuChanged(1);
  dumpModelInfo();

  TestRunner.addResult('Setting scale to 1.25');
  toolbar.onScaleMenuChanged(1.25);
  dumpModelInfo();

  TestRunner.completeTest();

  function dumpModelInfo() {
    TestRunner.addResult(
        'Scale: ' + model.scale() + ', appliedDeviceSize: ' + JSON.stringify(model.appliedDeviceSize()) +
        ', screenRect: ' + JSON.stringify(model.screenRect()) + ', visiblePageRect: ' +
        JSON.stringify(model.visiblePageRect()) + ', outlineRect: ' + JSON.stringify(model.outlineRect()));
  }
})();
