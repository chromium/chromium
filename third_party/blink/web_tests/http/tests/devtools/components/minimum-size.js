// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as UIModule from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`Tests how widget minimum size works.\n`);


  function showRootSplitWidget(splitWidget) {
    splitWidget.element.style.position = 'absolute';
    splitWidget.element.style.top = '0';
    splitWidget.element.style.left = '0';
    splitWidget.element.style.height = '500px';
    splitWidget.element.style.width = '500px';
    splitWidget.markAsRoot();
    splitWidget.show(document.body);
    return splitWidget;
  }

  function dumpBoundingBoxes(widgets) {
    for (var name in widgets) {
      var box = widgets[name].element.getBoundingClientRect();
      TestRunner.addResult(
          '[' + name + '] left = ' + box.left + '; right = ' + box.right + '; top = ' + box.top +
          '; bottom = ' + box.bottom);
    }
  }

  TestRunner.addResult('Creating simple hierarchy');
  var splitWidget = new UIModule.SplitWidget.SplitWidget(true, true, 'splitWidgetStateSettingName.splitWidget', 250, 250);
  showRootSplitWidget(splitWidget);

  var mainWidget = new UIModule.Widget.Widget();
  mainWidget.setMinimumSize(100, 80);
  splitWidget.setMainWidget(mainWidget);

  var firstSidebarWidget = new UIModule.Widget.Widget();
  firstSidebarWidget.setMinimumSize(40, 70);
  splitWidget.setSidebarWidget(firstSidebarWidget);

  var widgets = {'splitWidget': splitWidget, 'mainWidget': mainWidget, 'sidebarWidget': firstSidebarWidget};
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Changing sidebar size');
  splitWidget.setSidebarSize(30);
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Changing sidebar widget minimum size');
  firstSidebarWidget.setMinimumSize(90, 70);
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Changing orientation');
  splitWidget.setVertical(false);
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Changing main widget minimum size');
  mainWidget.setMinimumSize(450, 450);
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Changing main widget minimum size back and resizing');
  mainWidget.setMinimumSize(100, 80);
  splitWidget.setSidebarSize(450);
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Wrapping main widget to a split widget');
  var childsplitWidget = new UIModule.SplitWidget.SplitWidget(false, true, 'splitWidgetStateSettingName.childsplitWidget', 100, 100);
  childsplitWidget.hideSidebar();
  childsplitWidget.setMainWidget(mainWidget);
  splitWidget.setMainWidget(childsplitWidget);
  widgets['childSplitWidget'] = childsplitWidget;
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Adding invisble sidebar');
  var secondSidebarWidget = new UIModule.Widget.Widget();
  secondSidebarWidget.setMinimumSize(60, 60);
  childsplitWidget.setSidebarWidget(secondSidebarWidget);
  widgets['secondSidebarWidget'] = secondSidebarWidget;
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Showing sidebar');
  childsplitWidget.showBoth();
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Detaching sidebar');
  secondSidebarWidget.detach();
  delete widgets['secondSidebarWidget'];
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Attaching another sidebar');
  var thirdSidebarWidget = new UIModule.Widget.Widget();
  thirdSidebarWidget.setMinimumSize(80, 80);
  childsplitWidget.setSidebarWidget(thirdSidebarWidget);
  widgets['thirdSidebarWidget'] = thirdSidebarWidget;
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Swapping main and sidebar');
  splitWidget.setSecondIsSidebar(false);
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Arranging preferred sizes');
  firstSidebarWidget.setMinimumAndPreferredSizes(50, 50, 100, 100);
  mainWidget.setMinimumAndPreferredSizes(50, 50, 200, 200);
  thirdSidebarWidget.setMinimumAndPreferredSizes(49, 49, 99, 99);
  splitWidget.setSidebarSize(260);
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Less than sidebar preferred size');
  splitWidget.setSidebarSize(80);
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Size changes proportionally');
  splitWidget.setSidebarSize(320);
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Manual resize inside child split widget');
  childsplitWidget.setSidebarSize(50);
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Manual resize inside child split widget');
  childsplitWidget.setSidebarSize(120);
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Child split widget gets more space');
  splitWidget.setSidebarSize(170);
  dumpBoundingBoxes(widgets);

  TestRunner.addResult('Child split widget gets less space');
  splitWidget.setSidebarSize(360);
  dumpBoundingBoxes(widgets);

  TestRunner.completeTest();
})();
