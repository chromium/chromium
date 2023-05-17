// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {LayersTestRunner} from 'layers_test_runner';

(async function() {
  TestRunner.addResult(`Tests correctness of wheel scroll regions produced by SVG elements after parent update\n`);
  await TestRunner.loadHTML(`
    <div id="svg-root">
      <div>
        <svg width="100" height="100">
          <rect width="50" height="50" id="rect"></rect>
        </svg>
      </div>
      <div>
        <svg width="100" height="100">
          <image width="20" height="20" id="image" xlink:href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAFElEQVQIHWP4z8DwHwyBNJDN8B8AQNEG+t5Ik2kAAAAASUVORK5CYII="/>
        </svg>
      </div>
    </div>
    `);

  await TestRunner.evaluateInPageAsync(`
    (function() {
      var rect = document.getElementById("rect");
      rect.addEventListener("wheel", () => {}, false);
      var image = document.getElementById("image");
      image.addEventListener("wheel", () => {}, false);
      return new Promise(f => testRunner.updateAllLifecyclePhasesAndCompositeThen(f));
    })();
  `);

  await TestRunner.evaluateInPageAsync(`
    (function() {
      var svgRoot = document.getElementById("svg-root");
      svgRoot.style.transform = "translate(100px)";
      return new Promise(f => testRunner.updateAllLifecyclePhasesAndCompositeThen(f));
    })();
  `);

  await LayersTestRunner.requestLayers();

  TestRunner.addResult('Scroll rectangles');
  LayersTestRunner.layerTreeModel().layerTree().forEachLayer(layer => {
    if (layer.scrollRectsInternal.length > 0)
      TestRunner.addObject(layer.scrollRectsInternal);
  });
  TestRunner.completeTest();
})();
