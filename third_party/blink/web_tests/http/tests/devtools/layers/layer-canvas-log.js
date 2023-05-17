// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {LayersTestRunner} from 'layers_test_runner';

(async function() {
  TestRunner.addResult(`Tests layer command log\n`);
  await TestRunner.loadHTML(`
      <div id="a" style="will-change: transform; background-color:blue; width:100px; height:100px;">
          <div style="width:50px; height:50px; background-color:red;"></div>
          <img src="resources/test.png">
          <svg>
              <rect x="0" y="0" width="10" height="10" style="opacity:0.5"/>
          </svg>
      </div>
  `);

  await LayersTestRunner.requestLayers();

  var layer = LayersTestRunner.findLayerByNodeIdAttribute('a');
  layer.snapshots()[0].then(snapshotWithRect => snapshotWithRect.snapshot.commandLog()).then(onHistoryReceived);

  function onHistoryReceived(items) {
    TestRunner.addResult('Canvas log:');
    TestRunner.addObject(items, {'uniqueID': 'skip'});
    TestRunner.completeTest();
  }
})();
