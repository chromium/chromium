// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {LayersTestRunner} from 'layers_test_runner';

import * as Layers from 'devtools/panels/layers/layers.js';
import * as LayerViewer from 'devtools/panels/layer_viewer/layer_viewer.js';

(async function() {
  TestRunner.addResult(`Tests hit testing in Layers3DView\n`);

  await TestRunner.loadHTML(`
      <div id="a" style="will-change: transform; transform:translateY(60px) rotateZ(45deg);width:300px;height:300px;margin-left:100px; border: 1px solid black;">
          <div id="b" style="will-change: transform; transform:translateX(0px) translateY(0px) rotateX(45deg) rotateY(45deg);width:300px;height:300px; border: 1px solid black;"></div>
      </div>
  `);

  var contentRoot;
  var layers;
  var rootOffsetXInPixels, rootOffsetYInPixels, rootHeightInPixels, rootWidthInPixels;
  var canvas;
  const ButtonByEventType = {mousemove: -1, mousedown: 0, mouseup: 0};

  TestRunner.showPanel('layers');
  await LayersTestRunner.requestLayers();
  initLayers();
  initSizes();

  TestRunner.addResult('Initial state');
  dumpOutlinedStateForLayers();

  TestRunner.addResult('\nHovering content root');
  dispatchMouseEventOnCanvas('mousemove', 0.1237135833164431, 0.11536508236291274);
  dumpOutlinedStateForLayers();

  TestRunner.addResult('\nSelecting layer b');
  dispatchMouseEventOnCanvas('mousedown', 0.5, 0.5);
  dispatchMouseEventOnCanvas('mouseup', 0.5, 0.5);
  dumpOutlinedStateForLayers();

  TestRunner.addResult('\nHovering layer a');
  dispatchMouseEventOnCanvas('mousemove', 0.4, 0.2);
  dumpOutlinedStateForLayers();

  TestRunner.addResult('\nSelecting content root');
  dispatchMouseEventOnCanvas('mousedown', 0.5, 0.001);
  dispatchMouseEventOnCanvas('mouseup', 0.5, 0.001);
  dumpOutlinedStateForLayers();

  TestRunner.completeTest();

  function initLayers() {
    const layerA = LayersTestRunner.findLayerByNodeIdAttribute('a');
    const layerB = LayersTestRunner.findLayerByNodeIdAttribute('b');
    contentRoot = LayersTestRunner.layerTreeModel().layerTree().contentRoot();
    layers = [
      {layer: layerA, name: 'layer a'}, {layer: layerB, name: 'layer b'}, {layer: contentRoot, name: 'content root'}
    ];
  }

  function initSizes() {
    canvas = Layers.LayersPanel.LayersPanel.instance()._layers3DView._canvasElement;
    var canvasWidth = canvas.offsetWidth;
    var canvasHeight = canvas.offsetHeight;
    var rootWidth = 800;
    var rootHeight = 600;
    var paddingX = canvasWidth * 0.1;
    var paddingY = canvasHeight * 0.1;
    var scaleX = rootWidth / (canvasWidth - paddingX);
    var scaleY = rootHeight / (canvasHeight - paddingY);
    var resultScale = Math.max(scaleX, scaleY);
    var width = canvasWidth * resultScale;
    var height = canvasHeight * resultScale;
    var rootOffsetX = (width - rootWidth) / 2;
    var rootOffsetY = (height - rootHeight) / 2;
    rootOffsetXInPixels = rootOffsetX / width * canvasWidth;
    rootOffsetYInPixels = rootOffsetY / height * canvasHeight;
    rootHeightInPixels = rootHeight / height * canvasHeight;
    rootWidthInPixels = rootHeight / width * canvasWidth;
  }

  function dispatchMouseEventOnCanvas(eventType, x, y) {
    LayersTestRunner.dispatchMouseEvent(
        eventType, ButtonByEventType[eventType], canvas, rootOffsetXInPixels + rootWidthInPixels * x,
        rootOffsetYInPixels + rootHeightInPixels * y);
  }

  function dumpStateForOutlineType(type) {
    var outlined = 'none';
    Layers.LayersPanel.LayersPanel.instance()._update();

    function checkLayer(layerInfo) {
      var l3dview = Layers.LayersPanel.LayersPanel.instance()._layers3DView;
      if (l3dview._lastSelection[type] && layerInfo.layer.id() === l3dview._lastSelection[type].layer().id())
        outlined = layerInfo.name;
    }

    layers.forEach(checkLayer);
    TestRunner.addResult(type + ' layer: ' + outlined);
  }

  function dumpOutlinedStateForLayers() {
    TestRunner.addResult('State of layers:');
    dumpStateForOutlineType(LayerViewer.Layers3DView.Layers3DView.OutlineType.Hovered);
    dumpStateForOutlineType(LayerViewer.Layers3DView.Layers3DView.OutlineType.Selected);
  }

})();
