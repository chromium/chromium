// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {LayersTestRunner} from 'layers_test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`Tests accessibility in the layers view using the axe-core linter.`);
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
  await UI.ViewManager.ViewManager.instance().showView(view);
  const widget = await UI.ViewManager.ViewManager.instance().view(view).widget();

  await LayersTestRunner.requestLayers();

  const layer = LayersTestRunner.findLayerByNodeIdAttribute('a');
  const snapshotWithRect = await layer.snapshots()[0];
  await snapshotWithRect.snapshot.commandLog();
  await AxeCoreTestRunner.runValidation(widget.element);
  TestRunner.completeTest();
})();
