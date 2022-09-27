// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests accessibility in the layers view using the axe-core linter.`);
  await TestRunner.loadTestModule('layers_test_runner');
  await TestRunner.loadTestModule('axe_core_test_runner');
  await TestRunner.loadHTML(`
      <div id="a" style="transform: translateZ(0px); background-color:blue; width:100px; height:100px;">
          <div style="width:50px; height:50px; background-color:red;"></div>
          <img src="resources/test.png">
          <svg>
              <rect x="0" y="0" width="10" height="10" style="opacity:0.5"/>
          </svg>
      </div>
  `);
  const view = 'layers';
  await UI.viewManager.showView(view);
  const widget = await UI.viewManager.view(view).widget();

  await LayersTestRunner.requestLayers();

  const layer = LayersTestRunner.findLayerByNodeIdAttribute('a');
  const snapshotWithRect = await layer.snapshots()[0];
  await snapshotWithRect.snapshot.commandLog();
  await AxeCoreTestRunner.runValidation(widget.element);
  TestRunner.completeTest();
})();
