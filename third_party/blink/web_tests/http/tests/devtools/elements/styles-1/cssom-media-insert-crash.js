// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that the inspected page does not crash after inspecting element with CSSOM added rules. Bug 373508 crbug.com/373508\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      div {
          border: 1px solid black;
          background-color: white;
          padding: 20px;
      }
      </style>
      <div id="box">Inspecting this element crashes DevTools</div>
    `);
  await TestRunner.evaluateInPagePromise(`
      var lastSheet = document.styleSheets[document.styleSheets.length - 1];
      var mediaIndex = lastSheet.insertRule('@media all { }', lastSheet.cssRules.length);
      var mediaRule = lastSheet.cssRules[mediaIndex];
      mediaRule.insertRule('#box { background: red; color: white; }', mediaRule.cssRules.length);
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('box', step1);

  async function step1() {
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true, false);
    TestRunner.completeTest();
  }
})();
