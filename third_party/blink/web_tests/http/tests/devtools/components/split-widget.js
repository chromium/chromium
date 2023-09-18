// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as UIModule from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`Tests how split widget saving to settings works.\n`);


  var settingIndex = 0;
  function createAndShowSplitWidget(
      isVertical, secondIsSidebar, settingName, defaultSidebarWidth, defaultSidebarHeight, shouldSaveShowMode) {
    var splitWidget =
        new UIModule.SplitWidget.SplitWidget(isVertical, secondIsSidebar, settingName, defaultSidebarWidth, defaultSidebarHeight);
    splitWidget.setMainWidget(new UIModule.Widget.Widget());
    splitWidget.setSidebarWidget(new UIModule.Widget.Widget());
    if (shouldSaveShowMode)
      splitWidget.enableShowModeSaving();
    splitWidget.element.style.position = 'absolute';
    splitWidget.element.style.top = '0';
    splitWidget.element.style.left = '0';
    splitWidget.element.style.height = '500px';
    splitWidget.element.style.width = '500px';
    splitWidget.markAsRoot();
    splitWidget.show(document.body);
    return splitWidget;
  }

  function dumpSplitWidgetOrientationShowModeAndSidebarSize(splitWidget) {
    var sidebarSize = splitWidget.isVertical() ? splitWidget.sidebarWidget().element.offsetWidth :
                                                 splitWidget.sidebarWidget().element.offsetHeight;
    var orientation = splitWidget.isVertical() ? 'vertical' : 'horizontal';
    TestRunner.addResult(
        '    Sidebar size = ' + sidebarSize + ', showMode = ' + splitWidget.showMode() + ', ' + orientation);
    TestRunner.addResult(
        '    Setting value: ' + JSON.stringify(Common.Settings.settingForTest(splitWidget.setting.name).get()));
  }

  function testSplitWidgetSizes(useFraction, shouldSaveShowMode) {
    var secondIsSidebar = true;
    var settingName = 'splitWidgetStateSettingName' + (++settingIndex);
    var defaultSidebarWidth = useFraction ? 0.23 : 101;
    var defaultSidebarHeight = useFraction ? 0.24 : 102;
    var newWidth = useFraction ? 125 : 201;
    var newHeight = useFraction ? 130 : 202;

    var splitWidget;
    TestRunner.addResult('Create default split widget');
    var params = 'useFraction = ' + useFraction + ', shouldSaveShowMode = ' + shouldSaveShowMode;
    TestRunner.addResult('Running split widget test with the following parameters: ' + params);

    TestRunner.addResult('Creating split widget');
    splitWidget = createAndShowSplitWidget(
        true, secondIsSidebar, settingName, defaultSidebarWidth, defaultSidebarHeight, shouldSaveShowMode);
    dumpSplitWidgetOrientationShowModeAndSidebarSize(splitWidget);

    TestRunner.addResult('Hiding sidebar');
    splitWidget.hideSidebar();
    dumpSplitWidgetOrientationShowModeAndSidebarSize(splitWidget);

    TestRunner.addResult('Showing sidebar');
    splitWidget.showBoth();
    dumpSplitWidgetOrientationShowModeAndSidebarSize(splitWidget);

    TestRunner.addResult('Resizing');
    splitWidget.setSidebarSize(newWidth);
    dumpSplitWidgetOrientationShowModeAndSidebarSize(splitWidget);

    TestRunner.addResult('Hiding sidebar');
    splitWidget.hideSidebar();
    dumpSplitWidgetOrientationShowModeAndSidebarSize(splitWidget);

    TestRunner.addResult('Showing sidebar');
    splitWidget.showBoth();
    dumpSplitWidgetOrientationShowModeAndSidebarSize(splitWidget);

    TestRunner.addResult('Making horizontal');
    splitWidget.setVertical(false);
    dumpSplitWidgetOrientationShowModeAndSidebarSize(splitWidget);

    TestRunner.addResult('Resizing');
    splitWidget.setSidebarSize(newHeight);
    dumpSplitWidgetOrientationShowModeAndSidebarSize(splitWidget);

    TestRunner.addResult('Hiding sidebar');
    splitWidget.hideSidebar();
    dumpSplitWidgetOrientationShowModeAndSidebarSize(splitWidget);

    splitWidget.element.remove();

    TestRunner.addResult('Recreating split widget');
    splitWidget = createAndShowSplitWidget(
        true, secondIsSidebar, settingName, defaultSidebarWidth, defaultSidebarHeight, shouldSaveShowMode);
    dumpSplitWidgetOrientationShowModeAndSidebarSize(splitWidget);

    TestRunner.addResult('Hiding sidebar');
    splitWidget.hideSidebar();
    dumpSplitWidgetOrientationShowModeAndSidebarSize(splitWidget);

    TestRunner.addResult('Making horizontal');
    splitWidget.setVertical(false);
    dumpSplitWidgetOrientationShowModeAndSidebarSize(splitWidget);

    TestRunner.addResult('Showing sidebar');
    splitWidget.showBoth();
    dumpSplitWidgetOrientationShowModeAndSidebarSize(splitWidget);

    splitWidget.element.remove();

    TestRunner.addResult('Recreating split widget');
    splitWidget = createAndShowSplitWidget(
        true, secondIsSidebar, settingName, defaultSidebarWidth, defaultSidebarHeight, shouldSaveShowMode);
    dumpSplitWidgetOrientationShowModeAndSidebarSize(splitWidget);

    TestRunner.addResult('Making horizontal');
    splitWidget.setVertical(false);
    dumpSplitWidgetOrientationShowModeAndSidebarSize(splitWidget);

    splitWidget.element.remove();
    TestRunner.addResult('');
  }

  // Test all combinations of useFraction and shouldSaveShowMode flags
  testSplitWidgetSizes(false, false);
  testSplitWidgetSizes(false, true);
  testSplitWidgetSizes(true, false);
  testSplitWidgetSizes(true, true);
  TestRunner.completeTest();
})();
