// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that computed styles expand and allow tracing to style rules.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      body {
          background-color: rgb(100, 0, 0);
          font-family: Arial;
      }

      div {
          text-decoration: underline;
      }

      #id1 {
          background-color: green;
          font-family: Times;
      }

      #id1 {
          background-color: black;
          font-family: Courier;
      }

      #id1 {
          background: gray;
      }

      #id2 {
          background-color: blue;
          font-family: Courier;
          text-decoration: invalidvalue;
      }

      </style>
      <div id="id1">
      <div id="id2">
      </div>
      <button id="id3" hidden>
      </button></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('id1', step1);

  async function step1() {
    TestRunner.addResult('==== Computed style for ID1 ====');
    await ElementsTestRunner.dumpSelectedElementStyles(false, true);
    ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('id2', step2);
  }

  async function step2() {
    TestRunner.addResult('==== Computed style for ID2 ====');
    await ElementsTestRunner.dumpSelectedElementStyles(false, true);
    ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('id3', step3);
  }

  async function step3() {
    TestRunner.addResult('==== Style for ID3 ====');
    // The button[hidden] style specifies "display: none", which should not be /-- overloaded --/.
    await ElementsTestRunner.dumpSelectedElementStyles(true, true);
    TestRunner.completeTest();
  }
})();
