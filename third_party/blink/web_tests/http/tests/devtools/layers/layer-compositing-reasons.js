// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests layer compositing reasons in Layers Panel`);
  await TestRunner.loadTestModule('layers_test_runner');
  await TestRunner.navigatePromise(TestRunner.url('resources/compositing-reasons.html'));

  async function dumpCompositingReasons(layer) {
    const node = layer.nodeForSelfOrAncestor();
    if (node) {
      const label = Elements.DOMPath.fullQualifiedSelector(node, false);
      const reasonIds = await layer.requestCompositingReasonIds();
      TestRunner.addResult(`Compositing reason ids for ${label}: ` + reasonIds.sort().join(','));
    }
  }

  const idsToTest = [
    'transform3d', 'scale3d', 'rotate3d', 'translate3d', 'backface-visibility',
    'animation', 'animation-scale', 'animation-rotate', 'animation-translate',
    'transformWithCompositedDescendants', 'transformWithCompositedDescendants-individual',
    'opacityWithCompositedDescendants', 'reflectionWithCompositedDescendants', 'perspective', 'preserve3d'
  ];

  await LayersTestRunner.requestLayers();
  dumpCompositingReasons(LayersTestRunner.layerTreeModel().layerTree().contentRoot());
  for (let i = 0; i < idsToTest.length - 1; ++i) {
    dumpCompositingReasons(LayersTestRunner.findLayerByNodeIdAttribute(idsToTest[i]));
  }

  await dumpCompositingReasons(LayersTestRunner.findLayerByNodeIdAttribute(idsToTest[idsToTest.length - 1]));
  TestRunner.completeTest();
})();
