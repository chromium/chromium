// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that when layer snapshots are replayed with scaling applied the image dimensions are properly scaled.\n`);
  await TestRunner.loadModule('layers_test_runner');
  await TestRunner.loadHTML(`
      <div id="a" style="background-color:blue; will-change: transform; overflow: hidden;">

          <div style="width:50px; height:50px; background-color:red;"></div>
          <img src="../tracing/resources/test.png">
          <svg>
              <rect x="0" y="0" width="10" height="10" style="opacity:0.5"/>
          </svg>
        </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      (function() {
          var a = document.getElementById("a");
          var size = (180 / window.devicePixelRatio) + "px";
          a.style.width = size;
          a.style.height = size;
      })();
    `);

  await LayersTestRunner.requestLayers();

  var layer = LayersTestRunner.findLayerByNodeIdAttribute('a');
  var snapshotWithRect = await layer.snapshots()[0];
  await testImageForSnapshot(snapshotWithRect.snapshot, undefined);
  await testImageForSnapshot(snapshotWithRect.snapshot, 0.5);
  TestRunner.completeTest();

  async function testImageForSnapshot(snapshot, scale) {
    var imageURL = await snapshot.replay(scale);
    var image = await new Promise(fulfill => {
      var image = new Image();
      image.addEventListener('load', () => fulfill(image), false);
      image.src = imageURL;
    });
    TestRunner.addResult(`Image dimensions at scale ${scale}: ${image.naturalWidth} x ${image.naturalHeight}`);
  }
})();
