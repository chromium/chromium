// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {LayersTestRunner} from 'layers_test_runner';

import * as Layers from 'devtools/panels/layers/layers.js';

(async function() {
  TestRunner.addResult(`Tests moust hover/select events handling in the Layers panel\n`);
  await TestRunner.loadHTML(`
      <style>
      .layer {
          position: absolute;
          transform: translateZ(10px);
          opacity: 0.8;
          left: 20px;
          top: 20px;
          background-color: #eee;
          border-color: black;
      }
      </style>
      <div id="a" style="width: 200px; height: 200px" class="layer">
        <div class="layer" id="b1" style="width: 150px; height: 100px"></div>
        <div id="b2" class="layer" style="width: 140px; height: 110px">
          <div id="b3" class="layer" style="width: 140px; height: 110px;"></div>
          <div id="c" class="layer" style="width: 100px; height: 90px"></div>
        </div>
      </div>
    `);

  await TestRunner.showPanel('layers');
  await LayersTestRunner.requestLayers();

  Layers.LayersPanel.LayersPanel.instance().update();
  var layerB1 = LayersTestRunner.findLayerByNodeIdAttribute('b1');
  var treeElementB1 = LayersTestRunner.findLayerTreeElement(layerB1);

  var layerB3 = LayersTestRunner.findLayerByNodeIdAttribute('b3');
  var treeElementB3 = LayersTestRunner.findLayerTreeElement(layerB3);

  function dumpElementSelectionState() {
    Layers.LayersPanel.LayersPanel.instance().update();
    LayersTestRunner.dumpSelectedStyles('Layer b1 in tree', treeElementB1);
    LayersTestRunner.dumpSelectedStyles('Layer b3 in tree', treeElementB3);
  }
  TestRunner.addResult('Hovering b1 in tree');
  LayersTestRunner.dispatchMouseEventToLayerTree('mousemove', -1, layerB1);
  dumpElementSelectionState();

  TestRunner.addResult('Hovering b3 in tree');
  LayersTestRunner.dispatchMouseEventToLayerTree('mousemove', -1, layerB3);
  dumpElementSelectionState();

  TestRunner.addResult('Hovering away from tree');
  LayersTestRunner.dispatchMouseEventToLayerTree('mouseout', -1, layerB3);
  dumpElementSelectionState();

  TestRunner.addResult('Selecting b1 in tree');
  LayersTestRunner.dispatchMouseEventToLayerTree('mousedown', 0, layerB1);
  dumpElementSelectionState();

  TestRunner.addResult('Selecting b3 in tree');
  LayersTestRunner.dispatchMouseEventToLayerTree('mousedown', 0, layerB3);
  dumpElementSelectionState();

  TestRunner.completeTest();
})();
