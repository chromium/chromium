// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {LayersTestRunner} from 'layers_test_runner';

(async function() {
  TestRunner.addResult(`Tests scroll rectangles support in in Layers3DViewxScroll rectangles\n`);
  await TestRunner.loadHTML(`
      <div style="transform: translateZ(100px);height:20px;width:30px;" onmousewheel=""></div>
      <div id="touchable" style="transform:translateZ(100px);height:20px;width:20px;overflow:scroll;">
          <div style="height:40px;width:40px;"></div>
      </div>
    `);
  await TestRunner.evaluateInPageAsync(`
      (function() {
        var element = document.getElementById('touchable');
        element.addEventListener("touchstart", () => {}, false);
        return new Promise(f => testRunner.updateAllLifecyclePhasesAndCompositeThen(f));
      })();
    `);

  await LayersTestRunner.requestLayers();

  TestRunner.addResult('Scroll rectangles');
  LayersTestRunner.layerTreeModel().layerTree().forEachLayer(layer => {
    const scrollRects = layer.scrollRects();
    if (scrollRects.length > 0)
      TestRunner.addObject(scrollRects);
  });
  TestRunner.completeTest();

})();
