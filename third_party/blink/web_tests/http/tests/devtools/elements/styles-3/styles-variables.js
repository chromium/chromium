// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that computed styles expand and allow tracing to style rules.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      body {
          --a: red;
      }

      #id1 {
          --b: 44px;
      }

      #id2 {
          --a: green;
      }

      #id3 {
          --a: inherit;
      }

      #id4 {
          --a: var(--z);
      }

      #id5 {
          --a: var(--b);
          --b: var(--a);
      }

      </style>
      <div id="id1">
      <div id="id2">
      <div id="id3">
      </div>
      </div>
      </div>
      <div id="id4">
      </div>
      <div id="id5">
      </div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('id1', step1);
  async function step1(node) {
    TestRunner.addResult('==== Computed style for ID1 ====');
    await ElementsTestRunner.dumpSelectedElementStyles(false, false);
    TestRunner.cssModel.getComputedStyle(node.id).then(function(style) {
      TestRunner.addResult('value of --a: ' + style.get('--a'));
      ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('id2', step2);
    });
  }

  async function step2(node) {
    TestRunner.addResult('==== Computed style for ID2 ====');
    await ElementsTestRunner.dumpSelectedElementStyles(false, false);
    TestRunner.cssModel.getComputedStyle(node.id).then(function(style) {
      TestRunner.addResult('value of --b: ' + style.get('--b'));
      ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('id3', step3);
    });
  }

  async function step3(node) {
    TestRunner.addResult('==== Computed style for ID3 ====');
    await ElementsTestRunner.dumpSelectedElementStyles(false, false);
    TestRunner.cssModel.getComputedStyle(node.id).then(function(style) {
      TestRunner.addResult('value of --b: ' + style.get('--b'));
      ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('id4', step4);
    });
  }

  async function step4(node) {
    TestRunner.addResult('==== Computed style for ID4 ====');
    await ElementsTestRunner.dumpSelectedElementStyles(false, false);
    TestRunner.cssModel.getComputedStyle(node.id).then(function(style) {
      TestRunner.addResult('value of --a: ' + style.get('--a'));
      ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('id5', step5);
    });
  }

  async function step5(node) {
    TestRunner.addResult('==== Computed style for ID5 ====');
    await ElementsTestRunner.dumpSelectedElementStyles(false, false);
    TestRunner.cssModel.getComputedStyle(node.id).then(function(style) {
      TestRunner.addResult('value of --a: ' + style.get('--a'));
      TestRunner.completeTest();
    });
  }
})();
