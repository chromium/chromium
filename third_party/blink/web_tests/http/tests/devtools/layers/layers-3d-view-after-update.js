// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Layers after update in Layers3DView\n`);
  await TestRunner.loadModule('layers_test_runner');

  await TestRunner.loadHTML(`
      <style>
        #layer {
          will-change: transform;
          width: 300px;
          height: 300px;
          transform: translate(100px, 100px);
        }
      </style>
      <div id="layer"></div>
  `);

  TestRunner.showPanel('layers');
  await LayersTestRunner.requestLayers();
  var layer = LayersTestRunner.findLayerByNodeIdAttribute('layer');
  initialQuads = layer.quad().toString();

  // Updating layers should not produce invalid layer to-screen transforms
  // (see: https://crbug.com/977578). Backface visibility is changed, rather
  // than just adjusting the transform, to ensure fast-path optimizations do not
  // prevent a full layer tree update.
  await LayersTestRunner.evaluateAndWaitForTreeChange(
      'layer.style.backfaceVisibility = "hidden";');

  layer = LayersTestRunner.findLayerByNodeIdAttribute('layer');
  if (initialQuads === layer.quad().toString())
    TestRunner.addResult('Pass: Layer quads are unchanged by no-op update');
  else
    TestRunner.addResult('Fail: Layer quads are changed by no-op update');

  TestRunner.completeTest();
})();

