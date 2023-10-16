// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that elements panel shows DOM tree structure.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE html>
      <div id="level1">
        <div id="level2">
          &gt;&lt;&quot;'
          &nbsp;_&shy;_&ensp;_&emsp;_&thinsp;_&hairsp;_&ZeroWidthSpace;_&zwnj;_&zwj;_&lrm;_&rlm;_&#x202A;_&#x202B;_&#x202C;_&#x202D;_&#x202E;_&NoBreak;_&#xFEFF;
          <div id="level3"></div>
        </div>
      </div>
      <div id="replacement-character"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      document.querySelector("#replacement-character").textContent = "\uFEFF";
  `);

  // Warm up highlighter module.
  ElementsTestRunner.expandElementsTree(step1);

  function step1() {
    ElementsTestRunner.dumpElementsTree();
    TestRunner.completeTest();
  }
})();
