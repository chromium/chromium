// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests general layer tree model functionality`);
  await TestRunner.loadModule('layers_test_runner');
  await TestRunner.loadHTML(`
      <style>
      .layer {
          transform: translateZ(10px);
          opacity: 0.8;
          background: blue;
      }
      #frame {
          width: 200px;
          height: 200px;
      }
      </style>
      <div id="a" style="width: 200px; height: 200px" class="layer">
          <div class="layer" id="b1" style="width: 150px; height: 100px"></div>
          <div id="b2" class="layer" style="width: 140px; height: 110px">
              <div id="c" class="layer" style="width: 100px; height: 90px"></div>
          </div>
          <div id="b3" class="layer" style="width: 140px; height: 110px"></div>
      </div>
    `);
  await TestRunner.addIframe('resources/composited-iframe.html', {id: 'frame'});
  await TestRunner.evaluateInPagePromise(`
      function updateTree()
      {
          document.getElementById("c").appendChild(document.getElementById("b1"));
          var b3 = document.getElementById("b3");
          b3.parentElement.removeChild(b3);
          var b4 = document.createElement("div");
          b4.id = "b4";
          b4.className = "layer";
          b4.style.width = "77px";
          b4.style.height = "88px";
          document.getElementById("a").appendChild(b4);
      }

      function updateGeometry()
      {
          document.getElementById("c").style.width = "80px";
          // Simply changing the transform or transform-origin may not cause a
          // full layer update due to optimizations. Changing backface
          // visibility is a heavier hammer to force a full update.
          document.getElementById("c").style.backfaceVisibility = "hidden";
      }
  `);

  await LayersTestRunner.requestLayers();

  // Assure layer objects are not re-created during updates.
  LayersTestRunner.layerTreeModel().layerTree().forEachLayer(layer => {
    layer.__extraData = layer.parent() ? layer.parent().__extraData + 1 : 0;
  });

  TestRunner.addResult('Initial layer tree');
  LayersTestRunner.dumpLayerTree();
  await LayersTestRunner.evaluateAndWaitForTreeChange('requestAnimationFrame(updateTree)');

  TestRunner.addResult('Updated layer tree');
  LayersTestRunner.dumpLayerTree();
  await LayersTestRunner.evaluateAndWaitForTreeChange('requestAnimationFrame(updateGeometry)');

  TestRunner.addResult('Updated layer geometry');
  LayersTestRunner.dumpLayerTree();
  TestRunner.completeTest();
})();
