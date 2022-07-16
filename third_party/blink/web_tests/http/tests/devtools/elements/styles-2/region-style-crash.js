// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that webkit css region styling can be parsed correctly. Test passes if it doesn't crash.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #article1 { -webkit-flow-into: flow1; }
      #region1 { -webkit-flow-from: flow1; position: absolute; top: 10px; width: 350px; height: 25px;}
      #p1 { color: #ff0000; }
      @-webkit-region #region1 {
          #p1 { color: #008000; }
      }

      </style>
      <div id="article1">
          <p id="p1">P color styled in region: #008000.</p>
      </div>
      <div id="region1" class="regionBox"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('p1', dumpAllStyles);

  async function dumpAllStyles() {
    await ElementsTestRunner.dumpSelectedElementStyles();
    TestRunner.completeTest();
  }
})();
